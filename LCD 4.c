
#ifndef F_CPU
#define F_CPU 16000000UL // 16 MHz clock speed
#endif

#define BAUD 9600UL							// скорость передачи данных по UART
#define LINE_SIZE 16U						// длина строки дисплея 
#define DYSPLAY_SIZE 2U						// количество строк на экране 
#define BUFFER_SIZE 255U					// размер буфера приемника
#define RESPONSE "\nOK\n"					// ответ терминалу при нажатии клавиши Ввод
#define HELLO "\n Hello \n"

#define NEWLINE_CHR 0x0dU					// символ перевода строки
#define BACKSPACE_CHR 0x7fU					// символ забоя

#define D4 eS_PORTD4
#define D5 eS_PORTD5
#define D6 eS_PORTD6
#define D7 eS_PORTD7
#define RS eS_PORTB0
#define EN eS_PORTB1

// === объявление глобальных переменных и функций ===
// ==================================================
#define BAUDRATE ((F_CPU)/(BAUD*16UL)-1)    // макрос расчета скорости передачи для UBRR
#define A_SIZE(a)  (sizeof(a)/sizeof(*(a)))	// макрос расчета числа элементов массива

volatile char uartRxBuf = 0;				// однобайтный буфер UART
volatile char c_buf[BUFFER_SIZE + 1] = "\0";	// сдвиговый буфер UART
char *tx_data;								// указатель на данные для передачи
unsigned int tx_size = 0;					// размер данных для передачи
volatile unsigned int tx_i = 0;				// индекс данных передачи

#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>					// подключаем библиотеку для работы с прерываниями
#include <util/atomic.h>					// подключаем библиотеку для атомарных операций
#include <string.h>
#include "lcd.h"
#include <avr/io.h>

// инициализация UART
void uartInit (unsigned int baudrate)
{
	UBRR0H = (unsigned char)(baudrate>>8);	// сдвигаем число вправо на 8 бит
	UBRR0L = (unsigned char)baudrate;		// устанавливаем скорость передачи
	UCSR0B|= (1<<RXCIE0);				// разрешаем прерывание по приему
	UCSR0B|= (1<<TXEN0)|(1<<RXEN0);     // включаем приемник и передатчик
	UCSR0C|= (2<<UPM00)|(3<<UCSZ00);    // проверка на четность even parity (UPM1,UPM0), формат данных 8 бит
}

// запись символа в сдвиговый буфер
void pushChar(char c){
	unsigned int l, i;
	l = strlen((char*)c_buf);				// считываем кол-во символов в буфере
	if(l == BUFFER_SIZE){			// если буфер переполнен, то...
		for(i=0; i < BUFFER_SIZE - 1; i++){		// сдвигаем буфер влево на один символ
			c_buf[i] = c_buf[i+1];
		}
		c_buf[i] = c;				// в последнюю позицию записываем поступивший символ
		} else {						// если буфер не переполнен, то...
		c_buf[l] = c;						// добавляем в него поступивший символ
	}
}

// получение символа из сдвигового буфера
char shiftChar(void){
	unsigned int l, i;
	char c = '\0';					// если буфер пуст, вернем нуль
	ATOMIC_BLOCK(ATOMIC_FORCEON){	// выделяем блок кода, прерывания в котором запрещены
		l = strlen((char*)c_buf);			// считываем кол-во символов в буфере
		if(l){						// если буфер не пуст, то...
			c = c_buf[0];					// забираем символ из первой позиции
			if(l == 1){					// если в буфере только один символ, то...
				c_buf[0] = '\0';				// очищаем буфер
				} else {					// если в буфере несколько символов, то...
				for(i=0; i < l - 1; i++){	// сдвигаем буфер влево на один символ
					c_buf[i] = c_buf[i+1];
				}
				c_buf[l - 1] = '\0';			// последнюю позицию зануляем
			}
		}
	}
	return c;
}

// отправка символа на ПК
void sendData(char *data, int size){
	if(size){							// если данные есть, то...
		while((UCSR0B & (1<<UDRIE0)));	// ждем пока завершится предыдущая передача
		tx_data = data;					// записываем адрес данных для передачи в указатель
		tx_size = size;					// сохраняем размер данных для передачи в байтах
		tx_i = 0;						// обнуляем счетчик передачи
		UCSR0B|= (1<<UDRIE0);			// разрешаем прерывание по опустошению регистра передатчика
	}
}

ISR(USART_RX_vect)      // вектор прерывания UART - завершение приема
{
	uartRxBuf = UDR0;
	pushChar(uartRxBuf);
	uartRxBuf = 0;
}

ISR(USART_TX_vect)      // вектор прерывания UART - завершение передачи
{
	
}

ISR(USART_UDRE_vect)    // вектор прерывания UART - регистр данных на передачю пуст
{
	if(tx_i < tx_size){			// если данные на передачу ещё есть, то...
		UDR0 = tx_data[tx_i];	// передаем и
		tx_i++;					// инкрементируем счетчик передачи
		} else {
		UCSR0B &=~(1<<UDRIE0);	// если данные закончились, то запрещаем прерывание по опустошению регистра передатчика
	}
}


int main(void)
{
	char text[DYSPLAY_SIZE][LINE_SIZE + 1];					// строковый массив DYSPLAY_SIZE х LINE_SIZE,
															// в котором будут хранится строки дисплея для операций над ними ("сдвиг" экрана вверх, забой)
	char c[2] = "\0";										// сюда будем принимать символ
	char cl[LINE_SIZE + 1] = "\0";							// пустая строка длиной LINE_SIZE символов для очистки строки дисплея
	int line = 0;											// номер текущей строки
	int c_line = 1;											// строка символа курсора
	int pos = 0;											// текущая позиция в строке
	int c_pos = 0;											// позиция символа курсора в строке
	int bsf = 0;
	int i = 0;
	int l = 0;
	
	// === инициализация ===
	// =====================
	
	// выделяем память под массив строк
	memset(text, 0, sizeof(char) * DYSPLAY_SIZE * (LINE_SIZE + 1));
	
	// заполняем пустую строку пробелами
	for(i=0; i<LINE_SIZE; i++) {
		cl[i] = 0x20;
	}
	
	uartInit(BAUDRATE);				// инициализация UART
							
									// инициализация дисплея
	
	DDRD = 0xFF;
	DDRB = 0xFF;
	//////int i;
	Lcd4_Init();
	

	asm("sei");				// разрешаем прерывания
	
  Lcd4_Set_Cursor(1,1);
  Lcd4_Write_String("Hello My Lord!");
  sendData(HELLO, A_SIZE(HELLO));
  _delay_ms(2000);
  Lcd4_Clear();
  
   while(1)
   {
	   
	   
	   if ((c[0] = shiftChar())){	// если получен символ, то...
		   
		   if(c[0] == BACKSPACE_CHR){		// если это символ забоя, то...
			   
			   if(pos != 0){						// если курсор не в нулевой позиции строки, то...
				   pos--;								// сдвигаем курсор влево
				   bsf = 1;							// устанавливаем флаг забоя
				   
				   } else if(line != 0){				// если курсор в нулевой позиции не первой строки, то...
				   
				   line--;								// переводим курсор на строку выше
				   pos = LINE_SIZE - 1;					// устанавливаем курсор на последнюю позицию строки
				   bsf = 1;							// устанавливаем флаг забоя
			   }
			   
			   if(bsf){								// если установлен флаг забоя, то
				   bsf = 0;								// сбрасываем его и
				   l = strlen(text[line]);					// уменьшаем строку на один символ
				   text[line][l-1] = '\0';
				   Lcd4_Set_Cursor(line+1, 0);
				   Lcd4_Write_String(cl);			// очищаем строку на дисплее полностью
				   Lcd4_Set_Cursor(line+1, 0);
				   Lcd4_Write_String(text[line]);	// записываем новый текст в строку
			   }
			   
			   } else {
			   
			   if(c[0] == NEWLINE_CHR){	// если принят символ перевода строки
				   line++;						// переводим курсор на новую строку
				   pos= 0;
				   
				   sendData(RESPONSE, A_SIZE(RESPONSE));	// отправляем терминалу "OK"
				   
				   } else {					// если принят другой символ, то...
				   Lcd4_Set_Cursor(line+1, pos);   
				   Lcd4_Write_Char(c[0]);	// выводим и
				   strcat(text[line], c);				// сохраняем символ
				   
				   if(pos > LINE_SIZE - 2){		// если курсор на последней позиции, то...
					   line++;						// переводим курсор на новую строку
					   pos= 0;
					   } else {
					   pos++;						// или смещаем позицию курсора вправо
				   }
			   }
			   
			   if(line > 1 ){		// -1 если превышено ограничение используемых строк, то...
				   line--;							// переводим курсор на строку выше
		   
				    strcpy(text[0], text[1]);
					Lcd4_Clear();				// очищаем дисплей
					Lcd4_Set_Cursor(1, 0);
					Lcd4_Write_String(text[0]);
				   
				/*   for(i=0; i < DYSPLAY_SIZE-1 ; i++){		//-1 выводим строки, смещая "экран" вверх
					   strcpy(text[i+1], text[i]);
					   Lcd4_Set_Cursor((i+1), 0);
					   Lcd4_Write_String(text[i]);
				   }
				   */
				   strcpy(text[1], "\0");	// очищаем последнюю строку
			   }
		   }
		   
		   } else {
		   // отображаем мигающий курсор
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