#include <avr/pgmspace.h>
#include "Wire.h"

const int buf_max = 64;

typedef struct command_t
{
    char * name;
    void (*callback)(char, char **);
} command_t;

void test_command(char argc, char * argv[]);
void blink_command(char argc, char * argv[]);
void pwm_command(char argc, char * argv[]);
void status_command(char argc, char * argv[]);
void i2c_command(char argc, char * argv[]);

bool is_delim(char c)
{
    return(
        c == ' ' ||
        c == '\t' ||
        c == 0
        );
}

command_t * find_cmd(char * str)
{
    static const command_t command_list[] = 
    {
        {"test", &test_command},
        {"blink", &blink_command},
        {"restart", nullptr},
        {"pwm", &pwm_command},
        {"status", &status_command},
        {"i2c", &i2c_command},
    };
    
    for(char i = 0; i < sizeof(command_list)/sizeof(command_t); ++i)
    {
        if(strcmp(command_list[i].name, str) == 0)
        {
            return &command_list[i];
        }
    }
    return nullptr;
}

void parse_command(char * str, HardwareSerial & serial)
{
    char * argv[buf_max/2 + 1] = {0};
    char argc = 0;
    
    for(char i = 0; str[i] != 0; ++i)
    {
        if(
            (i == 0 && !is_delim(str[i])) ||
            (is_delim(str[i-1]) && !is_delim(str[i]))
        )
        {
            argv[argc++] = &(str[i]);
        }
        else if(is_delim(str[i]))
        {
            str[i] = 0;
        }

    }
    
    // do command call
    command_t * cmd_ptr = nullptr;
    if(cmd_ptr = find_cmd(argv[0]))
    {
        (*cmd_ptr).callback(argc, argv);
    }
    else if(argv[0] != 0)
    {
        serial.print("?: ");
        serial.println(argv[0]);
    }
}

void parse_serial(char * buf, int * buf_cnt, const int buf_max, HardwareSerial & serial, char * prompt, char * buf_prev)
{
    char tmp = serial.read();
//    serial.println(tmp, HEX);
    switch(tmp)
    {
        case '\r':
        {
            buf[*buf_cnt] = 0;
            serial.write(tmp);
            serial.write('\n');
            for(char i = 0; buf[i] != 0; ++i)
            {
                buf_prev[i] = buf[i];
                buf_prev[i+1] = 0;
            }
            parse_command(buf, serial);
            *buf_cnt = 0;
            serial.write(prompt);
            break;
        }
        case '\b':
        case 127: // DEL
        {
            if(*buf_cnt > 0)
            {
                --(*buf_cnt);
                serial.write(8);
                serial.write(' ');
                serial.write(8);
            }
            break;
        }
        case 3: // ETX
        {
            *buf_cnt = 0;
            serial.println();
            serial.write(tmp);
            serial.write(prompt);
            break;
        }
        case '\e': // escape
        {
            while(!serial.available());
            char tmp1 = serial.read();
            while(!serial.available());
            char tmp2 = serial.read();
            if(tmp1 == '[')
            {
                switch(tmp2)
                {
                    case 'A': // up arrow
                    {
                        serial.write("\r"); // home
                        serial.write("\e[2K"); // clear line
                        for(*buf_cnt = 0; buf_prev[*buf_cnt] != 0; ++(*buf_cnt))
                        {
                            buf[*buf_cnt] = buf_prev[*buf_cnt];
                        }
                        serial.write(prompt);
                        serial.write(buf_prev);
                        break;
                    }
                    case 'B': // down arrow
                    {
                        serial.write("\r"); // home
                        serial.write("\e[2K"); // clear line
                        *buf_cnt = 0;
                        serial.write(prompt);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
            break;
        }
        default:
        {
            if(*buf_cnt < buf_max)
            {
                buf[(*buf_cnt)++] = tmp;
                serial.write(tmp);
            }
            break;
        }
        case '\n':
        {
            // Throw it away
            break;
        }
    }
}

inline HardwareSerial & get_terminal()
{
    return Serial1;
}

void status_command(char argc, char * argv[])
{
    HardwareSerial & terminal = get_terminal();
    terminal.print("SREG: 0x");
    terminal.println(SREG, HEX);
    terminal.print("SP: 0x");
    terminal.println(SP, HEX);
//    terminal.print("PC: ");
//    terminal.print(PC, HEX);
}

uint16_t ledblink_period = 3000; // ms
uint16_t ledblink_period_half = ledblink_period/2; // ms

void test_command(char argc, char * argv[])
{
    HardwareSerial & terminal = get_terminal();
    terminal.println("Testing!");
    terminal.println("Args: ");
    for(char i = 0; i < argc; ++i)
    {
        terminal.print(i, DEC);
        terminal.print(") ");
        terminal.println(argv[i]);
    }
}

void blink_command(char argc, char * argv[])
{
    if(argc > 1)
    {
        ledblink_period = atoi(argv[1]);
        ledblink_period_half = ledblink_period/2;        
    }
    else
    {
        HardwareSerial & terminal = get_terminal();
        terminal.print("Usage: ");
        terminal.print(argv[0]);
        terminal.println(" <period_in_ms>");
    }
}

const int led2 = 10;

void pwm_command(char argc, char * argv[])
{
    static char state = 0;
    if(argc > 1)
    {
        if(strcmp("on", argv[1]) == 0)
        {
            analogWrite(led2, 255);
            state = 1;
        }
        else if(strcmp("off", argv[1]) == 0)
        {
            analogWrite(led2, 0);
            state = 0;
        }
        else if(strcmp("fade", argv[1]) == 0)
        {
            if(state == 0)
            {
                for(unsigned long i = 0; i < 1000; ++i)
                {
                    analogWrite(led2, i*255UL/1000UL);
                    delay(2);
                }
                state = 1;
            }
            else
            {
                for(unsigned long i = 1000; i > 0; --i)
                {
                    analogWrite(led2, i*255UL/1000UL);
                    delay(2);
                }
                state = 0;
            }
        }
        else
        {
            analogWrite(led2, atoi(argv[1]));
            state = 0;
        }
    }
}

void i2c_command(char argc, char * argv[])
{
    if(argc > 1)
    {
        if(strcmp("setup", argv[1]) == 0)
        {
            const static char addr = 0x3E;
            const static char timeout = 10;
#if 1 // 3 V mode
            const static char bytes[] = {0x38, 0x39, 0x14, 0x74, 0x54, 0x6F, 0x0C, 0x01, 0x0F};
#else // 5 V mode
            const static char bytes[] = {0x38, 0x39, 0x14, 0x74, 0x54, 0x6F, 0x0C, 0x01, 0x0F};
#endif
            for(int i = 0; i < sizeof(bytes)/sizeof(bytes[0]); ++i)
            {
                Wire.beginTransmission(addr);
                Wire.write(bytes[i]);
                Wire.endTransmission();
                delay(timeout);
            }
        }
        else
        {
            Wire.beginTransmission(0x3E);
            Wire.write(atoi(argv[0]));
            Wire.endTransmission();
            delay(10);
        }
    }
}

const int led = 9;
const int button = 5;
const char * prompt = "ARD> ";

unsigned long timestamp = 0;
unsigned long pwmstamp = 0;
unsigned char pwm_timeout = 8;
char buf[buf_max + 1];
char buf_prev[buf_max + 1];
int buf_cnt = 0;

void setup()
{
    timestamp = 0;
    pwmstamp = 0;
    pwm_timeout = 8;
    buf_cnt = 0;
    analogWrite(led2, 0); // Until we get a pull down on this driver

    pinMode(led, OUTPUT);
    pinMode(led2, OUTPUT);
    pinMode(button, INPUT);
    digitalWrite(led, HIGH);

    Wire.begin();

    HardwareSerial & terminal = get_terminal();
    terminal.begin(115200);
//    terminal.write(0x0C); // Form feed
    terminal.println();
    terminal.println("Arduino florper 1.0");
    terminal.write(prompt);
}

void loop()
{
    unsigned long now = millis();

    // blink the led
    if(now > timestamp + ledblink_period_half)
    {
        digitalWrite(led, !digitalRead(led));
        timestamp += ledblink_period_half;
    }

    // check for new commands
    HardwareSerial & terminal = get_terminal();
    if(terminal.available() > 0)
    {
        parse_serial(buf, &buf_cnt, buf_max, terminal, prompt, buf_prev);
    }
    if(now > pwmstamp + pwm_timeout)
    {
        char diff = (digitalRead(button)<<1)-1;
        static unsigned char pwmout = 0;
        if((diff > 0 && pwmout < 255) || (diff < 0 && pwmout > 0))
        {
            pwmout += diff;
            analogWrite(led2, pwmout);
        }
        pwmstamp += pwm_timeout;
    }
}

