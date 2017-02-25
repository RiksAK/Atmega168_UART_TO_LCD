
#ifndef F_CPU
#define F_CPU 16000000UL // 16 MHz clock speed
#endif

#define BAUD 9600UL							// �������� �������� ������ �� UART
#define LINE_SIZE 16U						// ����� ������ ������� 
#define DYSPLAY_SIZE 2U						// ���������� ����� �� ������ 
#define BUFFER_SIZE 255U					// ������ ������ ���������
#define RESPONSE "\nOK\n"					// ����� ��������� ��� ������� ������� ����
#define HELLO "\n Hello \n"

#define NEWLINE_CHR 0x0dU					// ������ �������� ������
#define BACKSPACE_CHR 0x7fU					// ������ �����

#define D4 eS_PORTD4
#define D5 eS_PORTD5
#define D6 eS_PORTD6
#define D7 eS_PORTD7
#define RS eS_PORTB0
#define EN eS_PORTB1

// === ���������� ���������� ���������� � ������� ===
// ==================================================
#define BAUDRATE ((F_CPU)/(BAUD*16UL)-1)    // ������ ������� �������� �������� ��� UBRR
#define A_SIZE(a)  (sizeof(a)/sizeof(*(a)))	// ������ ������� ����� ��������� �������

volatile char uartRxBuf = 0;				// ����������� ����� UART
volatile char c_buf[BUFFER_SIZE + 1] = "\0";	// ��������� ����� UART
char *tx_data;								// ��������� �� ������ ��� ��������
unsigned int tx_size = 0;					// ������ ������ ��� ��������
volatile unsigned int tx_i = 0;				// ������ ������ ��������

#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>					// ���������� ���������� ��� ������ � ������������
#include <util/atomic.h>					// ���������� ���������� ��� ��������� ��������
#include <string.h>
#include "lcd.h"
#include <avr/io.h>

// ������������� UART
void uartInit (unsigned int baudrate)
{
	UBRR0H = (unsigned char)(baudrate>>8);	// �������� ����� ������ �� 8 ���
	UBRR0L = (unsigned char)baudrate;		// ������������� �������� ��������
	UCSR0B|= (1<<RXCIE0);				// ��������� ���������� �� ������
	UCSR0B|= (1<<TXEN0)|(1<<RXEN0);     // �������� �������� � ����������
	UCSR0C|= (2<<UPM00)|(3<<UCSZ00);    // �������� �� �������� even parity (UPM1,UPM0), ������ ������ 8 ���
}

// ������ ������� � ��������� �����
void pushChar(char c){
	unsigned int l, i;
	l = strlen((char*)c_buf);				// ��������� ���-�� �������� � ������
	if(l == BUFFER_SIZE){			// ���� ����� ����������, ��...
		for(i=0; i < BUFFER_SIZE - 1; i++){		// �������� ����� ����� �� ���� ������
			c_buf[i] = c_buf[i+1];
		}
		c_buf[i] = c;				// � ��������� ������� ���������� ����������� ������
		} else {						// ���� ����� �� ����������, ��...
		c_buf[l] = c;						// ��������� � ���� ����������� ������
	}
}

// ��������� ������� �� ���������� ������
char shiftChar(void){
	unsigned int l, i;
	char c = '\0';					// ���� ����� ����, ������ ����
	ATOMIC_BLOCK(ATOMIC_FORCEON){	// �������� ���� ����, ���������� � ������� ���������
		l = strlen((char*)c_buf);			// ��������� ���-�� �������� � ������
		if(l){						// ���� ����� �� ����, ��...
			c = c_buf[0];					// �������� ������ �� ������ �������
			if(l == 1){					// ���� � ������ ������ ���� ������, ��...
				c_buf[0] = '\0';				// ������� �����
				} else {					// ���� � ������ ��������� ��������, ��...
				for(i=0; i < l - 1; i++){	// �������� ����� ����� �� ���� ������
					c_buf[i] = c_buf[i+1];
				}
				c_buf[l - 1] = '\0';			// ��������� ������� ��������
			}
		}
	}
	return c;
}

// �������� ������� �� ��
void sendData(char *data, int size){
	if(size){							// ���� ������ ����, ��...
		while((UCSR0B & (1<<UDRIE0)));	// ���� ���� ���������� ���������� ��������
		tx_data = data;					// ���������� ����� ������ ��� �������� � ���������
		tx_size = size;					// ��������� ������ ������ ��� �������� � ������
		tx_i = 0;						// �������� ������� ��������
		UCSR0B|= (1<<UDRIE0);			// ��������� ���������� �� ����������� �������� �����������
	}
}

ISR(USART_RX_vect)      // ������ ���������� UART - ���������� ������
{
	uartRxBuf = UDR0;
	pushChar(uartRxBuf);
	uartRxBuf = 0;
}

ISR(USART_TX_vect)      // ������ ���������� UART - ���������� ��������
{
	
}

ISR(USART_UDRE_vect)    // ������ ���������� UART - ������� ������ �� �������� ����
{
	if(tx_i < tx_size){			// ���� ������ �� �������� ��� ����, ��...
		UDR0 = tx_data[tx_i];	// �������� �
		tx_i++;					// �������������� ������� ��������
		} else {
		UCSR0B &=~(1<<UDRIE0);	// ���� ������ �����������, �� ��������� ���������� �� ����������� �������� �����������
	}
}


int main(void)
{
	char text[DYSPLAY_SIZE][LINE_SIZE + 1];					// ��������� ������ DYSPLAY_SIZE � LINE_SIZE,
															// � ������� ����� �������� ������ ������� ��� �������� ��� ���� ("�����" ������ �����, �����)
	char c[2] = "\0";										// ���� ����� ��������� ������
	char cl[LINE_SIZE + 1] = "\0";							// ������ ������ ������ LINE_SIZE �������� ��� ������� ������ �������
	int line = 0;											// ����� ������� ������
	int c_line = 1;											// ������ ������� �������
	int pos = 0;											// ������� ������� � ������
	int c_pos = 0;											// ������� ������� ������� � ������
	int bsf = 0;
	int i = 0;
	int l = 0;
	
	// === ������������� ===
	// =====================
	
	// �������� ������ ��� ������ �����
	memset(text, 0, sizeof(char) * DYSPLAY_SIZE * (LINE_SIZE + 1));
	
	// ��������� ������ ������ ���������
	for(i=0; i<LINE_SIZE; i++) {
		cl[i] = 0x20;
	}
	
	uartInit(BAUDRATE);				// ������������� UART
							
									// ������������� �������
	
	DDRD = 0xFF;
	DDRB = 0xFF;
	//////int i;
	Lcd4_Init();
	

	asm("sei");				// ��������� ����������
	
  Lcd4_Set_Cursor(1,1);
  Lcd4_Write_String("Hello My Lord!");
  sendData(HELLO, A_SIZE(HELLO));
  _delay_ms(2000);
  Lcd4_Clear();
  
   while(1)
   {
	   
	   
	   if ((c[0] = shiftChar())){	// ���� ������� ������, ��...
		   
		   if(c[0] == BACKSPACE_CHR){		// ���� ��� ������ �����, ��...
			   
			   if(pos != 0){						// ���� ������ �� � ������� ������� ������, ��...
				   pos--;								// �������� ������ �����
				   bsf = 1;							// ������������� ���� �����
				   
				   } else if(line != 0){				// ���� ������ � ������� ������� �� ������ ������, ��...
				   
				   line--;								// ��������� ������ �� ������ ����
				   pos = LINE_SIZE - 1;					// ������������� ������ �� ��������� ������� ������
				   bsf = 1;							// ������������� ���� �����
			   }
			   
			   if(bsf){								// ���� ���������� ���� �����, ��
				   bsf = 0;								// ���������� ��� �
				   l = strlen(text[line]);					// ��������� ������ �� ���� ������
				   text[line][l-1] = '\0';
				   Lcd4_Set_Cursor(line+1, 0);
				   Lcd4_Write_String(cl);			// ������� ������ �� ������� ���������
				   Lcd4_Set_Cursor(line+1, 0);
				   Lcd4_Write_String(text[line]);	// ���������� ����� ����� � ������
			   }
			   
			   } else {
			   
			   if(c[0] == NEWLINE_CHR){	// ���� ������ ������ �������� ������
				   line++;						// ��������� ������ �� ����� ������
				   pos= 0;
				   
				   sendData(RESPONSE, A_SIZE(RESPONSE));	// ���������� ��������� "OK"
				   
				   } else {					// ���� ������ ������ ������, ��...
				   Lcd4_Set_Cursor(line+1, pos);   
				   Lcd4_Write_Char(c[0]);	// ������� �
				   strcat(text[line], c);				// ��������� ������
				   
				   if(pos > LINE_SIZE - 2){		// ���� ������ �� ��������� �������, ��...
					   line++;						// ��������� ������ �� ����� ������
					   pos= 0;
					   } else {
					   pos++;						// ��� ������� ������� ������� ������
				   }
			   }
			   
			   if(line > 1 ){		// -1 ���� ��������� ����������� ������������ �����, ��...
				   line--;							// ��������� ������ �� ������ ����
		   
				    strcpy(text[0], text[1]);
					Lcd4_Clear();				// ������� �������
					Lcd4_Set_Cursor(1, 0);
					Lcd4_Write_String(text[0]);
				   
				/*   for(i=0; i < DYSPLAY_SIZE-1 ; i++){		//-1 ������� ������, ������ "�����" �����
					   strcpy(text[i+1], text[i]);
					   Lcd4_Set_Cursor((i+1), 0);
					   Lcd4_Write_String(text[i]);
				   }
				   */
				   strcpy(text[1], "\0");	// ������� ��������� ������
			   }
		   }
		   
		   } else {
		   // ���������� �������� ������
		   if(pos > LINE_SIZE - 1){
			   c_pos = 0;
			   c_line = line + 2;
			   } else {
			   c_pos = pos;
			   c_line = line+1;
		   }
		   Lcd4_Set_Cursor(c_line, c_pos);
		   Lcd4_Write_Char('_');
		   _delay_ms(200);
		   Lcd4_Set_Cursor(c_line, c_pos);
		   Lcd4_Write_Char(' ');
		   _delay_ms(200);
	   }
	   
	  /* Lcd4_Set_Cursor(1,3);
	   Lcd4_Write_String("Vse SUPER!");
	    _delay_ms(1000);
	   for(i=0;i<15;i++)
	   {
		   _delay_ms(800);
		   Lcd4_Shift_Left();
	   }
	   for(i=0;i<15;i++)
	   {
		   _delay_ms(800);
		   Lcd4_Shift_Right();
	   }
	    _delay_ms(1000);
	   Lcd4_Clear();
	   Lcd4_Set_Cursor(2,2);
	   Lcd4_Write_String("SUPER SUPER!!");
	  
	   _delay_ms(5000);
	   */
   }
}