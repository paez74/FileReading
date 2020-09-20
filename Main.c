#include <stdio.h>
#include <string.h>
 
int main()
{
     char string[] = ":f 1";
     char dest[20] = "";
     memmove(string,string+3,strlen(string));
     printf("%s",string);
}