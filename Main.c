#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
//Gustavo Paez Villalobos A01039751
// Rafa Mtz a01039710
// Christopher Parga
// Grupo 3

void detectComand(char *comando){
  int decision = 0;
  if (strcmp(comando,":q") == 0)
  {
    decision = 1;
  } else if (strcmp(comando, ":f") == 0)
  {
    decision = 2;
  }
  
  switch (decision)
  {
  case 1:
  // Aqui iria funcion f que se encarga de contar el numero de veces que aparece la palabra en el texto
      printf("leyo comando F");
    break;

  case 2:
  // Aqui iria la 
      printf("leyo comando ");
  break;
  
  default:
    break;
  }
}


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
    
    detectComand(command);
  
  }

  if(!strcmp(command,":wq")){
    printf("Salir y guardar archivo\n");

    fclose(fptr);
    if(!strcmp(command,":wq")){
      printf("Salir y guardar archivo\n");

    }

  }