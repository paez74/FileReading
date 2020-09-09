#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
//Gustavo Paez Villalobos A01039751
// Rafa Mtz
// Christopher Parga
// Grupo 3



int main() {
  int num;
  FILE *fptr;
  char textName[20];
  char command[20];
  int numLines = 0;
  char (*lines)[50] = NULL;
  

  printf( "Please write the name of the file: \n" );
  gets(textName);

  fptr = fopen(textName,"rb");

  if(fptr == NULL){
    printf("El archivo no existe\n");
    fclose(fptr);
    exit(1);
  }

  if (!(lines = malloc (40 * sizeof *lines))) { 
        return 0;
    }
  
  //Logica para leer archivo linea por linea 
  while ( fgets(lines[numLines], 50, fptr)) {
        char *ptr = lines[numLines];
        for(; *ptr && *ptr != '\n';ptr++){}
        *ptr = 0;
        numLines++;
    }

  printf("%d\n",numLines);
  fclose (fptr); 
  
  for(int i = 0; i < numLines; i++)
  {
    printf("%s\n",lines[i]);
  }

  while(strcmp(command,":q") && strcmp(command,":wq")){
    gets(command); 
    printf("%s\n",command);
    
    /* 
    Aqui va la logica(funcion) que dependiendo el comando es la otra funcion que correria
    */
    

  }

  if(!strcmp(command,":wq")){
    printf("Salir y guardar archivo\n");

  }


}