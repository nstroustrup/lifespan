
  #include <stdio.h>
  int main(int, char **)
  {
    const double correct = 89255.0/1e22;
    printf("89255.0/1e22 = %g", correct);
    if( correct != 89255e-22 )
      {
      // correction required
      return 0;
      }
    return 1;
  }