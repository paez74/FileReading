#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
# define SIZE 30
//Gustavo Paez Villalobos A01039751
// Rafa Mtz
// Christopher Parga
// Grupo 3



void main() {
  int num;
  FILE *fptr;
  char textName[20];
  char command[20];
  bool flag = true;
  printf( "Please write the name of the file: \n" );
  gets(textName);

  fptr = fopen(textName,"rb");
  if(fptr == NULL){
    printf("El archivo no existe\n");
    fclose(fptr);
    exit(1);
  }

  /* 
  Aqui va la logica para leer archivo linea por linea y guardar lineas en un array
  */
  while(strcmp(command,":q") && strcmp(command,":wq")){
    gets(command); 
    printf("%s\n",command);
    
    /* 
    Aqui va la logica(funcion) que dependiendo el comando es la otra funcion que correria
    */
    

  }


  fclose(fptr);
  printf("%s\n",command);
  if(!strcmp(command,":wq")){
    printf("Salir y guardar archivo\n");
  }

    

}