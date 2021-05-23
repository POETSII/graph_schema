#include "../include/network.hpp"

#include "../include/generate_utils.hpp"

#include <unistd.h>
#include <iostream>


int main()
{
   std::mt19937_64 rng;

   for(unsigned w=1; w<20; w++){
       for(unsigned k=1; k<=w; k++){
           for(unsigned r=0; r<10; r++){
               create_exact_connections(w, k, rng);
           }
       }
   }
}
