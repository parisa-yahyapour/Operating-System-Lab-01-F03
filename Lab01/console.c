// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void consputc(int);
#define KEY_LF 0xE4
#define KEY_RT 0xE5
#define KEY_UP 0xE2
#define KEY_DN 0xE3
static int panicked = 0;
static int back_step = 0;

static struct
{
  struct spinlock lock;
  int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if (sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do
  {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  if (sign)
    buf[i++] = '-';

  while (--i >= 0)
    consputc(buf[i]);
}
// PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if (locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint *)(void *)(&fmt + 1);
  for (i = 0; (c = fmt[i] & 0xff) != 0; i++)
  {
    if (c != '%')
    {
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if (c == 0)
      break;
    switch (c)
    {
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if ((s = (char *)*argp++) == 0)
        s = "(null)";
      for (; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if (locking)
    release(&cons.lock);
}

void panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for (i = 0; i < 10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for (;;)
    ;
}

// PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort *)P2V(0xb8000); // CGA memory

static void
cgaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT + 1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT + 1);

  if (c == '\n')
    pos += 80 - pos % 80;
  else if (c == BACKSPACE)
  {
    if (pos > 0)
      --pos;
  }
  else
    crt[pos++] = (c & 0xff) | 0x0700; // black on white

  if (pos < 0 || pos > 25 * 80)
    panic("pos under/overflow");

  if ((pos / 80) >= 24)
  { // Scroll up.
    memmove(crt, crt + 80, sizeof(crt[0]) * 23 * 80);
    pos -= 80;
    memset(crt + pos, 0, sizeof(crt[0]) * (24 * 80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT + 1, pos >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT + 1, pos);
  crt[pos] = ' ' | 0x0700;
}

void consputc(int c)
{
  if (panicked)
  {
    cli();
    for (;;)
      ;
  }

  if (c == BACKSPACE)
  {
    uartputc('\b');
    uartputc(' ');
    uartputc('\b');
  }
  else
    uartputc(c);
  cgaputc(c);
}

#define INPUT_BUF 128
struct
{
  char buf[INPUT_BUF];
  uint r; // Read index
  uint w; // Write index
  uint e; // Edit index
} input;

#define C(x) ((x) - '@') // Control-x

static void activate_back_arrow()
{
  int cursor_position;
  outb(CRTPORT, 14);
  cursor_position = inb(CRTPORT + 1) << 8;
  outb(CRTPORT, 15);
  cursor_position |= inb(CRTPORT + 1);
  // check to make sure we dont go further than the input string so we shouldnt go left
  if (crt[cursor_position - 2] != (('$' & 0xff) | 0x0700))
  {
    cursor_position--;
  }
  outb(CRTPORT, 14);
  outb(CRTPORT + 1, cursor_position >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT + 1, cursor_position);
  back_step++; // remember how much we move
}

static void activate_forward_arrow()
{
  int cursor_position;
  outb(CRTPORT, 14);
  cursor_position = inb(CRTPORT + 1) << 8;
  outb(CRTPORT, 15);
  cursor_position |= inb(CRTPORT + 1);
  // if we dont have any input string we shouldnt be able to go right
  if (crt[cursor_position + 2] != (('$' & 0xff) | 0x0700))
  {
    cursor_position++;
  }
  outb(CRTPORT, 14);
  outb(CRTPORT + 1, cursor_position >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT + 1, cursor_position);
  back_step--; // remember how much we go right
}

static struct operation
{
  int num1;
  int num2;
  int operator;
  int existence_num1;
  int equal;
  int number_of_char;
  int existence_operator;
  int existence_num2;
} operation_default = {0, 0, 0, 0, 0, 0, 0, 0};

int find_number_digits(int number)
{
  int counter = 0;
  while (number >= 0)
  {
    number = number - 10;
    counter++;
  }
  return counter;
}

int display_result(float x)
{
  int integer_part = (int)x;
  int number_disp = find_number_digits(integer_part);
  int float_part = (x - integer_part) * 10;
  if (float_part == 0)
  {
    release(&cons.lock);
    cprintf("%d", integer_part);
    acquire(&cons.lock);
  }
  else
  {
    number_disp += 2;
    release(&cons.lock);

    cprintf("%d.%d", integer_part, float_part);
    acquire(&cons.lock);
  }
  return number_disp;
}
int static calculate_result()
{
  float result;
  switch (operation_default.operator)
  {
  case 42:
    result = operation_default.num1 * operation_default.num2;
    break;
  case 43:
    result = operation_default.num1 + operation_default.num2;
    break;
  case 45:
    result = operation_default.num1 - operation_default.num2;
    break;
  case 47:
    result = (float)operation_default.num1 / (float)operation_default.num2;
    break;
  default:
    result = 0;
    break;
  }
  return display_result(result);
}

void clear_the_struct()
{
  operation_default.num1 = 0;
  operation_default.num2 = 0;
  operation_default.existence_num1 = 0;
  operation_default.operator= 0;
  operation_default.equal = 0;
  operation_default.number_of_char = 0;
  operation_default.existence_operator = 0;
  operation_default.existence_num2 = 0;
}

void static arithmetic_replace(char c)
{
  int number = (int)c;
  if ((c <= '9' && c >= '0') && (operation_default.existence_operator == 0))
  {
    operation_default.num1 = operation_default.num1 * 10 + (int)c - (int)'0';
    operation_default.number_of_char++;
    operation_default.existence_num1 = 1;
  }
  else if ((number == 37 || number == 42 || number == 43 || number == 47 || number == 45) && (operation_default.existence_num1 == 1))
  {
    operation_default.operator= number;
    operation_default.number_of_char++;
    operation_default.existence_operator = 1;
  }
  else if ((c <= '9' && c >= '0') && (operation_default.existence_operator == 1))
  {
    operation_default.num2 = operation_default.num2 * 10 + (int)c - (int)'0';
    operation_default.number_of_char++;
    operation_default.existence_num2 = 1;
  }
  else if (c == '=' && operation_default.existence_num2 == 1)
  {
    operation_default.equal = 1;
    operation_default.number_of_char++;
  }
  else if (c == '?' && operation_default.equal == 1)
  {
    operation_default.number_of_char++;
    for (int i = 0; i < operation_default.number_of_char; i++)
    {
      input.e--;
      consputc(BACKSPACE);
    }
    int the_out = calculate_result();
    for (int i = 0; i < the_out; i++)
    {
      input.e++;
    }
    clear_the_struct();
  }
  else
  {
    clear_the_struct();
  }
}
struct Command
{
  char command_text[128];
  int command_length;
};

struct Command history[11];
// char history[11][128];
int num_command = 0;
int current_num_command = -1;

void print_t()
{
  for (int i = num_command - 1; i >= 0; i--)
  {
    cprintf(history[i].command_text);
    consputc('\n');
  }
}

int compare_string(char *str1, char *str2)
{
  if (strlen(str1) != strlen(str2))
  {
    return 1;
  }

  for (int i = 0; i < strlen(str1); i++)
  {
    if (str1[i] != str2[i])
    {
      return 1;
    }
  }
  return 0;
}

void shift_array()
{
  for (int i = 1; i < 11; i++)
  {
    memset(history[i - 1].command_text, '\0', 128);
    safestrcpy(history[i - 1].command_text, history[i].command_text, 128);
    history[i - 1].command_length = history[i].command_length;
    history[i].command_length = 0;
  }
  memset(history[10].command_text, '\0', 128);
  history[10].command_length = 0;
}
void add_to_history(int strart_pointer, int end_pointer)
{
  int index = 0;
  int lenght = 0;
  for (int i = strart_pointer; i < end_pointer; i++)
  {
    if (input.buf[i % INPUT_BUF] != '\n' && input.buf[i % INPUT_BUF] != '\r')
    {
      history[num_command].command_text[index] = input.buf[i];
      index++;
      lenght++;
    }
  }
  history[num_command].command_length = lenght;
}
#define UP 0
#define DOWN 1

void add_to_buffer(char text[128], int length)
{
  int index = input.r % INPUT_BUF;
  for (int i = 0; i < length; i++)
  {
    input.buf[index] = text[i];
    index++;
    input.e++;
  }
  input.e++;
}

void clear_current_command(int lenght)
{
  for (int i = 0; i < lenght; i++)
  {
    consputc(BACKSPACE);
    input.e--;
  }
}

void show_previous_command(int direction)
{
  if (current_num_command == num_command)
  {
    // input.e = input.r - 1;
    // current_num_command = -1;
    return;
  }
  if (current_num_command == -1 && direction == UP)
  {
    current_num_command = num_command - 1;
  }
  else
  {
    switch (direction)
    {
    case UP:
      if (current_num_command == 0)
      {
        return;
      }
      clear_current_command(history[current_num_command].command_length);
      current_num_command--;
      break;
    case DOWN:
      if (current_num_command == num_command)
      {
        input.e = input.r - 1;
        //current_num_command = -1;
        return;
      }
      clear_current_command(history[current_num_command].command_length);
      current_num_command++;
      break;
    default:
      current_num_command = -1;
      break;
    }
  }
  release(&cons.lock);
  cprintf(history[current_num_command].command_text);
  add_to_buffer(history[current_num_command].command_text, history[current_num_command].command_length);
  acquire(&cons.lock);
}

static void b_change_cursor_pos()
{
  int cursor_position;
  outb(CRTPORT, 14);
  cursor_position = inb(CRTPORT + 1) << 8;
  outb(CRTPORT, 15);
  cursor_position |= inb(CRTPORT + 1);
  // check to make sure we dont go further than the input string so we shouldnt go left
  if (crt[cursor_position - 2] != (('$' & 0xff) | 0x0700))
  {
    cursor_position--;
  }
  outb(CRTPORT, 14);
  outb(CRTPORT + 1, cursor_position >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT + 1, cursor_position);
}

void consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;
  static int capturing = 0;           // New variable: 1 if capturing input, 0 otherwise
  static char capture_buf[INPUT_BUF]; // Buffer to store captured input
  static int capture_idx = 0;         // Index to track position in capture buffer
  acquire(&cons.lock);
  while ((c = getc()) >= 0)
  {
    switch (c)
    {
    case C('P'): // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;
    case C('U'): // Kill line.
      while (input.e != input.w &&
             input.buf[(input.e - 1) % INPUT_BUF] != '\n')
      {
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case C('H'):
    case '\x7f': // Backspace
      if (input.e != input.w)
      {
        input.e--;
        consputc(BACKSPACE);
        capture_idx--;
      }
      break;
    case C('S'):       // Start capturing input (Ctrl + S)
      capturing = 1;   // Start capturing
      capture_idx = 0; // Reset the capture buffer index
      break;

    case C('F'):                            // Stop capturing and print (Ctrl + F)
      capturing = 0;                        // Stop capturing
      capture_buf[capture_idx] = '\0';      // Null-terminate the captured string
      release(&cons.lock);                  // release lock before printing
      for (int i = 0; i < capture_idx; i++) // Print captured text
      {
        consputc((char)capture_buf[i]);
      }
      acquire(&cons.lock); // Re-acquire the lock after printing
      break;
    case KEY_LF:
      if ((input.e - back_step) > input.w)
        activate_back_arrow();
      break;
    case KEY_RT:
      if (back_step > 0)
      {
        activate_forward_arrow();
      }
      break;
    case KEY_UP:
      input.e--;
      show_previous_command(UP);
      break;
    case KEY_DN:
      input.e--;
      show_previous_command(DOWN);
      break;
    default:
      if (capturing && capture_idx < INPUT_BUF - 1) // If we're capturing, store the input into capture_buf
      {
        if (c == 0) // Ignore releasing keys
        {
          break;
        }
        capture_buf[capture_idx] = c;
        capture_idx += 1;
      }
      if (c != 0 && input.e - input.r < INPUT_BUF)
      {
        if (back_step != 0)
        {
          int number = input.e;
          for (int i = number + 1; i > number - back_step; i--)
          {
            input.buf[i] = input.buf[i - 1];
            input.e--;
          }
          input.buf[number - back_step] = c;
          int current = input.e;
          for (int i = current + 1; i < number + 1; i++)
          {
            consputc(input.buf[i]);
            input.e++;
          }
          for (int i = 0; i < back_step; i++)
          {
            b_change_cursor_pos();
          }
          input.e++;
        }
        else
        {
          if (num_command == 11)
          {
            shift_array();
            num_command--;
          }
          c = (c == '\r') ? '\n' : c;
          input.buf[input.e++ % INPUT_BUF] = c;
          consputc(c);
          arithmetic_replace(c);
          if (c == '\n' || c == C('D') || input.e == input.r + INPUT_BUF)
          {
            current_num_command = -1;
            add_to_history(input.r % INPUT_BUF, input.e % INPUT_BUF);
            if (compare_string(history[num_command].command_text, "history") == 0)
            {
              release(&cons.lock);
              print_t();
              acquire(&cons.lock);
              input.e = input.r;
              if (input.e - input.r < INPUT_BUF)
              {
                input.buf[input.e++ % INPUT_BUF] = '\n';
              }
            }
            if (num_command != 11)
            {
              num_command++;
            }
            input.w = input.e;
            wakeup(&input.r);
            back_step = 0; // every time we go to next line every thing restart
          }
        }
      }
      break;
    }
  }
  release(&cons.lock);
  if (doprocdump)
  {
    procdump(); // now call procdump() wo. cons.lock held
  }
}

int consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while (n > 0)
  {
    while (input.r == input.w)
    {
      if (myproc()->killed)
      {
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if (c == C('D'))
    { // EOF
      if (n < target)
      {
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if (c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for (i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  ioapicenable(IRQ_KBD, 0);
}
