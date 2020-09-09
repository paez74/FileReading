#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
# define SIZE 30
//Gustavo Paez Villalobos A01039751
// Rafa Mtz
// Christopher Parga
// Grupo 3

// Funcion para contar el total de lineas
int countlines(char *filename)
{
                                  
  FILE *fp = fopen(filename,"r");
  int ch=0;
  int lines=0;

  if (fp == NULL)
  return 0;

  lines++;
  while ((ch = fgetc(fp)) != EOF)
    {
      if (ch == '\n')
    lines++;
    }
  fclose(fp);
  return lines;
}

void main() {
  int num;
  FILE *fptr;
  char textName[20];
  char command[20];
  char ch;
  char * line = NULL;
  size_t len = 0;
  size_t read;
  
  printf( "Please write the name of the file: \n" );
  gets(textName);

  fptr = fopen(textName,"rb");
  if(fptr == NULL){
    printf("El archivo no existe\n");
    fclose(fptr);
    exit(1);
  }
  const int numLines = countlines(textName);
  
  
  //Logica para leer archivo linea por linea 
  while ((read = getline(&line, &len, fptr)) != -1) {
        printf("Retrieved line of length %zu:\n", read);
        // To do añadir el arreglo aqui
        
    }
  while(strcmp(command,":q") && strcmp(command,":wq")){
    gets(command); 
    printf("%s\n",command);
    
    /* 
    Aqui va la logica(funcion) que dependiendo el comando es la otra funcion que correria
    */
    

  }


  fclose(fptr);
  if(!strcmp(command,":wq")){
    printf("Salir y guardar archivo\n");

  }


}