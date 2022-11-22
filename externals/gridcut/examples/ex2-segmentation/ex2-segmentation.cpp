/*
  ex2-segmentation
  ================

  This example shows a minimal image segmentation code based on GridCut.
*/

#include <cstdio>
#include <cmath>
#include <iostream>
#include <chrono> 
using namespace std::chrono; 

#include "../include/Image.h"
#include "../../include/GridCut/GridGraph_2D_4C.h"
//#include "../../include/GridCut/GridGraph_2D_4C_MT.h"

#define K 1000
#define SIGMA2 0.012f
#define WEIGHT(A) (short)(1+K*std::exp((-(A)*(A)/SIGMA2)))

#define RED  RGB(1,0,0)
#define BLUE RGB(0,0,1)

int main(int argc,char** argv)
{
  typedef GridGraph_2D_4C<short,short,int> Grid;
  //typedef GridGraph_2D_4C_MT<short,short,int> Grid;

  const Image<float> image = imread<float>("image.png");
  const Image<RGB> scribbles = imread<RGB>("scribbles.png");
  
  const int width  = image.width();
  const int height = image.height();

  Grid* grid = new Grid(width,height);
  //Grid* grid = new Grid(width,height,8,16);
  
  for (int y=0;y<height;y++)
  {
    for (int x=0;x<width;x++)
    {
      grid->set_terminal_cap(grid->node_id(x,y),
                             scribbles(x,y)==BLUE ? K : 0,
                             scribbles(x,y)==RED  ? K : 0);

      if (x<width-1)
      {
        const short cap = WEIGHT(image(x,y)-image(x+1,y));

        grid->set_neighbor_cap(grid->node_id(x  ,y),+1,0,cap);
        grid->set_neighbor_cap(grid->node_id(x+1,y),-1,0,cap);
      }

      if (y<height-1)
      {
        const short cap = WEIGHT(image(x,y)-image(x,y+1));

        grid->set_neighbor_cap(grid->node_id(x,y  ),0,+1,cap);
        grid->set_neighbor_cap(grid->node_id(x,y+1),0,-1,cap);
      }
    }
  }

  auto start = high_resolution_clock::now(); 
      grid->compute_maxflow();
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<microseconds>(stop - start); 
  std::cout << "microseconds: " << duration.count() << std::endl; 

  Image<RGB> output(width,height);

  for (int y=0;y<height;y++)
  {
    for (int x=0;x<width;x++)
    {
      output(x,y) = image(x,y)*(grid->get_segment(grid->node_id(x,y)) ? RED : BLUE);
    }
  }

  delete grid;

  imwrite(output,"output.png");  

  printf("The result was written to \"output.png\".\n");
  
  return 0;
}
