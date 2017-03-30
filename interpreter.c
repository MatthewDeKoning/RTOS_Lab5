#include "string.h"
#include "interpreter.h"
#include "ST7735.h"
//#include "Clock.h"
#include "UART.h"
#include "os.h"
#include "ff.h"
static char strArray[ROWS][COLS];

int8_t interpreter_line = 0;
int8_t interpreter_device = 0;
char interpreter_msg[21];
/* the error message when an invalid command was passed */
const char static * errorMsg = "Invalid Command";
const static char * messageOne = "MS: ";
const static char * messageTwo = "Clk Cycles: ";
static char string[100];
char msg1[100];
char msg2[100];
char *msgs[4]; 
char * out0 = "mystery";
char * out1 = "periodic task complete";
char * out2 = "periodic task enabled";
char * out3 = "thread switch";
char num[4];
void printDiagnostics(){
  uint32_t * times, * ms, *types;
  int  j;
  msgs[0] = out0;
  msgs[1] = out1;
  msgs[2] = out2;
  msgs[3] = out3;
  
  times = getTimes();
  ms = getMS();
  types = getTypes();
  for(j = 0; j < 4; j++){ msg1[j] = messageOne[j];}
  for(j = 0; j < 12; j++){ msg2[j] = messageTwo[j];}
  for(j = 0; j < 100; j++){
    itoa(j, num, 10, 0);
    UART_OutString(num);
    UART_OutString("  ");
    itoa(ms[j], msg1, 10, 4);
    UART_OutString(msg1);
    UART_OutString("  ");
    itoa(times[j], msg2, 10, 12);
    UART_OutString(msg2);
    UART_OutString("  ");
    UART_OutString(msgs[types[j]]);
    OutCRLF();
  }
}

void printMaxDI(void){
  int out = 0;//OS_MaxTimeDI();
  UART_OutString("Max Time with Interrupts disabled ");
  UART_OutUDec(out);
  OutCRLF();
}
/*
static void print_file(char file[]){
  int size, i;
  char * ptr;
  size = eFile_ROpen(file, &ptr);
  while(size != 0){
    for(i = 0; i < size; i++){
      
      UART_OutChar(*ptr);
      ptr++;
      
    }
    size = eFile_ReadNext(&ptr);
  }
}

static void print_dir(){
  char * ptr, * temp;
  char str[8];
  int j;
  uint32_t test;
  ptr = eFile_Directory();
  ptr+=6;
  temp = ptr;
  j = 0;
  while((*ptr) != '\0'){
    temp = ptr;
    while(temp != '\0' && j < 7){
      str[j] = *temp++;
      j++;
    }
    str[j] = '\0';
    j = 0;
    UART_OutString("File: ");
    UART_OutString(str);
    UART_OutChar('\n');
    UART_OutChar('\r');
    UART_OutString("Starting sector: ");
    UART_OutUDec((*(ptr+8)<<8) + *(ptr+9));
    UART_OutChar('\n');
    UART_OutChar('\r');
    UART_OutString("Size (Bytes): ");
    
    test = 0;
    test+= *(ptr+13)&0xFF;
    test+= (unsigned)(*(ptr+12)<<8)&0xFF00;
    test+= (unsigned)(*(ptr+11)<<16)&0xFF0000;
    test+= (unsigned)(*(ptr+10)<<24)&0xFF000000;
    UART_OutUDec(test);
    UART_OutChar('\n');
    UART_OutChar('\r');
    UART_OutChar('\n');
    UART_OutChar('\r');
    ptr+=14;
  }
}
*/
//The function to run as interpreter thread
void INTERPRETER_Run(){
  while(1){
#if DEBUG
    Debug_Task(7);
#endif //DEBUG

    UART_OutString(">");
    //OS_DisableInterpreter();
    UART_InString(string, 99);
    INTERPRETER_parseMessage(string);
    OutCRLF();
    if(interpreter_device == -1){
      if(interpreter_line == -2){
        printDiagnostics();
      }
      else if(interpreter_line == -3){
        printMaxDI();
      }
      else if(interpreter_line == -4){ //dir
        //print_dir();
      }
      else if(interpreter_line == -5){ //format
        //f_mkfs(args);
      }
      else if(interpreter_line == -6){ //prnt file
        //print_file(strArray[1]);
      }
      else if(interpreter_line == -7){ // delete file
        //eFile_Delete(strArray[1]);
        
      }
      else if(interpreter_line == -8){ //create file
        //if(eFile_Create(strArray[1]) >= 0){
          //UART_OutString("Success\n");
        //}
        //el/se{
          //UART_OutString("Failure\n");
        //}
        
      }
      else if(interpreter_line == -9){
        /*if(eFile_WOpen(strArray[1]) == 0){
          UART_OutString("File Open\n");
        }
        else{
          UART_OutString("No such file");
        }*/
      }
      else if(interpreter_line == -10){
        /*if(eFile_WriteString(strArray[1]) !=0){
          UART_OutString("Write Error");
        }*/
      }
      else if(interpreter_line == -11){
        //eFile_WClose();
      }
      else{
        UART_OutString(interpreter_msg);
      }
    }
    else{
      LCD_MailBox_Send(interpreter_device, interpreter_line, interpreter_msg);
    }
    OutCRLF();
  }
}

void INTERPRETER_initArray(){
  int i;
  for(i = 0; i < ROWS; i++){
    strArray[i][0] = '\0';
  }
}
void INTERPRETER_clearMsg(){
  int i;
  for(i = 0; i < 21; i++)
    interpreter_msg[i] = '\0';
}

void INTERPRETER_parseMessage(char* str){
  INTERPRETER_clearMsg();
  const char* delim = " ";
  interpreter_device = 0; interpreter_line = 0;
  strtoklist((const char*)str, strArray , delim);
  //INTERPRETER_printParams();
  int command = INTERPRETER_handleCommand(INTERPRETER_handleParam1(&interpreter_device, &interpreter_line));
  
  //int screenFlag = 1;
  //if(interpreter_device == -1){
    //screenFlag = 0;
  //}
  //ST7735_ds_SetCursor(interpreter_device, 0, interpreter_line);
  int i = 0;
  switch(command){
    case 1:
      //calculator
      interpreter_msg[0] = 'A';interpreter_msg[1] = 'N';interpreter_msg[2] = 'S';interpreter_msg[3] = ' ';interpreter_msg[4] = '=';interpreter_msg[5] = ' ';
      itoa(INTERPRETER_calculator(), interpreter_msg, 10, 6);
      break;
    case 2:
      //adc
      interpreter_msg[0] = 'A';interpreter_msg[1] = 'D';interpreter_msg[2] = 'C';interpreter_msg[3] = ' ';interpreter_msg[4] = '=';interpreter_msg[5] = ' ';
      //itoa(ADC_GetVoltage(), interpreter_msg, 10, 6);
      break;
    case 3:
      //timer
      interpreter_msg[0] = 'A';interpreter_msg[1] = 'v';interpreter_msg[2] = 'g';interpreter_msg[3] = 'r';interpreter_msg[4] = 't';interpreter_msg[5] = ':';interpreter_msg[6] = ' ';
      //itoa(OS_AvgRunTime(), interpreter_msg, 10, 7);
      break;
    case 4:
      //print string
      for(i = 0; i < LengthOfString(strArray[1]);i++){
        interpreter_msg[i] = strArray[1][i];
      }
      interpreter_msg[i+1] = '\0';
      break;
    case 5:
      //adjust screen
      //ST7735_ds_InitR(INITR_REDTAB, strToInt(strArray[1]), strToInt(strArray[2]), strToInt(strArray[3]),strToInt(strArray[4]));
      //interpreter_msg[0] = '\0';
      break;
    case 6:
      //Toggle Real Time ADC
      //interpreter_msg[0] = '\0';
      //ST7735_ds_SetCursor(interpreter_device, 0, interpreter_line);
      //ST7735_ds_OutString(i, "                    ");
      //ADC_RTVoltageToggle(interpreter_device,interpreter_line);
      break;
    case 7:
      //print time
      //Clock_GetTime(interpreter_msg);
      break;
    case 8:
      //set date
      //Clock_SetDay(strToInt(strArray[2]));
      //Clock_SetMonth(strToInt(strArray[1]));
      //Clock_SetYear(strToInt(strArray[3]));
      //interpreter_msg[0] = 'D'; interpreter_msg[1] = 'a';interpreter_msg[2] = 't';interpreter_msg[3] = 'e';interpreter_msg[4] = ' ';interpreter_msg[5] = 's';interpreter_msg[6] = 'e';interpreter_msg[7] = 't';
      //interpreter_msg[8] = '\0'; 
      break;
    case 9:
      //set time
      //Clock_SetHour(strToInt(strArray[1]));
      //Clock_SetMinute(strToInt(strArray[2]));
      //Clock_SetSecond(strToInt(strArray[3]));
      //interpreter_msg[0] = 'T'; interpreter_msg[1] = 'i';interpreter_msg[2] = 'm';interpreter_msg[3] = 'e';interpreter_msg[4] = ' ';interpreter_msg[5] = 's';interpreter_msg[6] = 'e';interpreter_msg[7] = 't';
      //interpreter_msg[8] = '\0'; 
      break;
    case 10:
      //print date
      //Clock_GetDate(interpreter_msg);
      break;
    case 11:
      //ST7735_ds_SetCursor(interpreter_device, 0, interpreter_line);
      //ST7735_ds_OutString(i, "                    ");
      //Clock_RTClockToggle(interpreter_device, interpreter_line, 1);
      //interpreter_msg[0] = '\0';
      break;
    case 12:
      //ST7735_ds_SetCursor(interpreter_device, 0, interpreter_line);
      //ST7735_ds_OutString(i, "                    ");
      //Clock_RTClockToggle(interpreter_device, interpreter_line, 2);
      //interpreter_msg[0] = '\0';
      break;
    case 13:
      //interpreter_msg[0] = 'L';interpreter_msg[1] = 'n';interpreter_msg[2] = 'g';interpreter_msg[3] = 'r';interpreter_msg[4] = 't';interpreter_msg[5] = ':';interpreter_msg[6] = ' ';
      //itoa(OS_LongRunTime(), interpreter_msg, 10, 7);
      break;
    case 14:
      //interpreter_msg[0] = 'S';interpreter_msg[1] = 'm';interpreter_msg[2] = 'l';interpreter_msg[3] = 'r';interpreter_msg[4] = 't';interpreter_msg[5] = ':';interpreter_msg[6] = ' ';
      //itoa(OS_ShortRunTime(), interpreter_msg, 10, 7);
      break;
    case 15:
       interpreter_line = -2;
       break;
    case 16:
      interpreter_line = -3;
      break;
    case 17:
      interpreter_line = -4; //dir
      break;
    case 18:
      interpreter_line = -5; //format
      break;
    case 19:
      interpreter_line = -6; //print file
      break;
    case 20:
      interpreter_line = -7; //delete file
      break;
    case 21:
      interpreter_line = -8;
      break;
    case 22:
      interpreter_line = -9;
      break;
    case 23:
      interpreter_line = -10;
      break;
    case 24:
      interpreter_line = -11;
      break;
    default:
      for(i = 0; i < LengthOfString(errorMsg); i++){
        interpreter_msg[i] = errorMsg[i];
      }
      break;
  }
  
}
static int stack[15];
int32_t INTERPRETER_calculator(){
  int i;int total;int index;
  for(i = 0; i < ROWS; i++){
    if(strArray[i][0] == '\0')
      break;
  }
  total = i;
  i = 0;
  stack[i] = '\0';
  index = 0;
  while(i < total){
    uint8_t op = INTERPRETER_detectOperator(strArray[i][0]);
    if(op == 0xff || ((int)strArray[i][1] >=48 && (int)strArray[i][1] <= 57)){
      stack[index] = strToInt(strArray[i]);
      index++;
    }
    else{
      if(index >= 2){
        int value = 0;
        switch (op){
          case 0:
            value = stack[index-2] + stack[index-1];
            stack[index] = 0;
            index--;
            stack[index-1] = value;
            break;
          case 1:
            value = stack[index-2] - stack[index-1];
            stack[index] = 0;
            index--;
            stack[index-1] = value;
            break;
          case 2:
            value = stack[index-2] * stack[index-1];
            stack[index] = 0;
            index--;
            stack[index-1] = value;
            break;
          case 3:
            value = stack[index-2] / stack[index-1];
            stack[index] = 0;
            index--;
            stack[index-1] = value;
            break;
        }
      }
      else{
        return 0;
      }
    }
    i++;
  }
  return stack[0];
}
void INTERPRETER_fixArray0(uint8_t index){
  int i;
  for(i = 0; i < (LengthOfString(strArray[0]) - index); i++){
    strArray[0][i] = strArray[0][index+i];
  }
  strArray[0][i] = '\0';
}
uint8_t INTERPRETER_handleCommand(uint8_t index){
  int num = 1;
  int i;
  for(i = 0; i < (LengthOfString(strArray[0]) - index); i++){
    if(((int)strArray[0][index+i] <48 || (int)strArray[0][index+i] > 57) && strArray[0][index+i] != '-' ){
      num = 0;
      break;
    }
  }
  if(num){
    INTERPRETER_fixArray0(index);
    return 1;
  }
  switch (LengthOfString(strArray[0]) - index){
    case 2:
        if(strArray[0][index] == 'j' && strArray[0][index+1] == 't'){
          return 16;
        }
        return 0xff;
        break;
    case 3:
      if(strArray[0][index] == 'a' && strArray[0][index+1] == 'd' && strArray[0][index+2] == 'c'){
        return 2;
      }
      else if(strArray[0][index] == 'd' && strArray[0][index+1] == 'i' && strArray[0][index+2] == 'r'){
        return 17;
      }
      else if(strArray[0][index] == 'd' && strArray[0][index+1] == 'e' && strArray[0][index+2] == 'l'){
        return 20;
      }
      return 0xff;
    case 4:
      if(strArray[0][index] == 'd' && strArray[0][index+1] == 'a' && strArray[0][index+2] == 't' && strArray[0][index+3] == 'e'){
        return 10;
      }
      else if(strArray[0][index] == 't' && strArray[0][index+1] == 'i' && strArray[0][index+2] == 'm' && strArray[0][index+3] == 'e'){
        return 7;
      }
      else if(strArray[0][index] == 'o' && strArray[0][index+1] == 'u' && strArray[0][index+2] == 't' && strArray[0][index+3] == 'f'){
        return 19;
      }
      else if(strArray[0][index] == 'o' && strArray[0][index+1] == 'p' && strArray[0][index+2] == 'e' && strArray[0][index+3] == 'n'){
        return 22;
      }
      
      return 0xff;
    case 5:
      if(strArray[0][index] == 'p' && strArray[0][index+1] == 'r' && strArray[0][index+2] == 'i' && strArray[0][index+3] == 'n' && strArray[0][index+4] == 't'){
        return 4;
      }
      else if(strArray[0][index] == 'a' && strArray[0][index+1] == 'd' && strArray[0][index+2] == 'c' && strArray[0][index+3] == 'o' && strArray[0][index+4] == 'n'){
        return 6;
      }
      else if(strArray[0][index] == 'w' && strArray[0][index+1] == 'r' && strArray[0][index+2] == 'i' && strArray[0][index+3] == 't' && strArray[0][index+4] == 'e'){
        return 23;
      }
      else if(strArray[0][index] == 'c' && strArray[0][index+1] == 'l' && strArray[0][index+2] == 'o' && strArray[0][index+3] == 's' && strArray[0][index+4] == 'e'){
        return 24;
      }
      return 0xff;
    case 6:
      if(strArray[0][index] == 's' && strArray[0][index+1] == 'c' && strArray[0][index+2] == 'r' && strArray[0][index+3] == 'e' && strArray[0][index+4] == 'e'&& strArray[0][index+5] == 'n'){
        return 5;
      }
      else if(strArray[0][index] == 'a' && strArray[0][index+1] == 'd' && strArray[0][index+2] == 'c' && strArray[0][index+3] == 'o' && strArray[0][index+4] == 'f'&& strArray[0][index+5] == 'f'){
        return 6;
      }
      else if(strArray[0][index] == 'f' && strArray[0][index+1] == 'o' && strArray[0][index+2] == 'r' && strArray[0][index+3] == 'm' && strArray[0][index+4] == 'a'&& strArray[0][index+5] == 't'){
        return 18;
      }
      else if(strArray[0][index] == 'c' && strArray[0][index+1] == 'r' && strArray[0][index+2] == 'e' && strArray[0][index+3] == 'a' && strArray[0][index+4] == 't'&& strArray[0][index+5] == 'e'){
        return 21;
      }
      return 0xff;
    case 7:
      if(strArray[0][index] == 's' && strArray[0][index+1] == 'e' && strArray[0][index+2] == 't' && strArray[0][index+3] == 'd' && strArray[0][index+4] == 'a'&& strArray[0][index+5] == 't' && strArray[0][index+6] == 'e'){
        return 8;
      }
      else if(strArray[0][index] == 's' && strArray[0][index+1] == 'e' && strArray[0][index+2] == 't' && strArray[0][index+3] == 't' && strArray[0][index+4] == 'i'&& strArray[0][index+5] == 'm' && strArray[0][index+6] == 'e'){
        return 9;
      }
      else if(strArray[0][index] == 'r' && strArray[0][index+1] == 'u' && strArray[0][index+2] == 'n' && strArray[0][index+3] == 't' && strArray[0][index+4] == 'i'&& strArray[0][index+5] == 'm' && strArray[0][index+6] == 'e'){
        return 3;
      }
      return 0xff;
    case 8:
      if(strArray[0][index] == 's' && strArray[0][index+1] == 'h' && strArray[0][index+2] == 'o' && strArray[0][index+3] == 'w' && strArray[0][index+4] == 't' && strArray[0][index+5] == 'i'&& strArray[0][index+6] == 'm' && strArray[0][index+7] == 'e'){
        return 11;
      }
      else if(strArray[0][index] == 's' && strArray[0][index+1] == 'h' && strArray[0][index+2] == 'o' && strArray[0][index+3] == 'w' && strArray[0][index+4] == 'd' && strArray[0][index+5] == 'a' && strArray[0][index+6] == 't' && strArray[0][index+7] == 'e'){
        return 12;
      }
      else if(strArray[0][index] == 'r' && strArray[0][index+1] == 'u' && strArray[0][index+2] == 'n' && strArray[0][index+3] == 't' && strArray[0][index+4] == 'i'&& strArray[0][index+5] == 'm' && strArray[0][index+6] == 'e' && strArray[0][index+7] == 'l'){
        return 13;
      }
      else if(strArray[0][index] == 'r' && strArray[0][index+1] == 'u' && strArray[0][index+2] == 'n' && strArray[0][index+3] == 't' && strArray[0][index+4] == 'i'&& strArray[0][index+5] == 'm' && strArray[0][index+6] == 'e' && strArray[0][index+7] == 's'){
        return 14;
      }
      return 0xff;
    case 11:
      if(strArray[0][index] == 'd' && strArray[0][index+1] == 'i' && strArray[0][index+2] == 'a' && strArray[0][index+3] == 'g' && strArray[0][index+4] == 'n'&& strArray[0][index+5] == 'o' && strArray[0][index+6] == 's' && strArray[0][index+7] == 't'&& strArray[0][index+8] == 'i'&& strArray[0][index+9] == 'c'&& strArray[0][index+10] == 's'){
        return 15;
      }
    default:
      return 0xff;
  }
}
uint8_t   INTERPRETER_handleParam1(int8_t* interpreter_device, int8_t* interpreter_line){
  
  switch (firstIndex(strArray[0], ":")) {
    case 0:
      *interpreter_device = -1;
      *interpreter_line = -1;
      return 1;
      break;
    case 1:
      //TO SCREEN 0 interpreter_line 0
      return 2;
      break;
    case 2:
      //TO SCREEN interpreter_line[1] interpreter_line 0
      *interpreter_device = charToInt(strArray[0][1]);
      return 3;
      break;
      case 4:
      //TO SCREEN interpreter_line[1] interpreter_line interpreter_line[3]
      *interpreter_device = charToInt(strArray[0][1]);
      *interpreter_line = charToInt(strArray[0][3]);
      return 5;
      break;
  }
  
}

uint8_t INTERPRETER_detectOperator(char c){
  switch(c){
    case '+':
      return 0;
      break;
    case '-':
      return 1;
      break;
    case '*':
      return 2;
      break;
    case '/':
      return 3;
    default:
      return 0xff;
      break;
  }
}
/*void INTERPRETER_printParams(){
  int i =0;
  for(i = 0; i < ROWS; i++){
    if(strArray[i][0] != '\0'){
      printf("%s\n", strArray[i]);
    }
  }
  printf("\n");
}*/