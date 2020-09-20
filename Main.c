#include <stdio.h>
#include <string.h>
 
int main()
{
     char string[] = ":r Un uno";
     char dest[20] = "";
     char delim[] = " ";
    strcpy(dest,string);
    memmove(dest,dest+3,strlen(dest));
    char *ptr = strtok(dest,delim);
    char *toReplace, *replacement;
    toReplace = ptr;
    printf("toReplace %s ", toReplace);
    ptr = strtok(NULL,delim);
    replacement = ptr;
    printf("replacement %s ",replacement);
}