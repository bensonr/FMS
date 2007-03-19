#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifdef use_libMPI 
#include <mpi.h>
#endif
#include "mosaic_util.h"
#include "constant.h"

#define HPI (0.5*M_PI)
#define TPI (2.0*M_PI)
#define TOLORENCE (1.e-6)
/***********************************************************
    void error_handler(char *str)
    error handler: will print out error message and then abort
***********************************************************/

void error_handler(const char *msg)
{
  fprintf(stderr, "FATAL Error: %s\n", msg );
#ifdef use_libMPI      
  MPI_Abort(MPI_COMM_WORLD, -1);
#else
  exit(1);
#endif  
}; /* error_handler */

/*********************************************************************

   int nearest_index(double value, const double *array, int ia)

   return index of nearest data point within "array" corresponding to "value".
   if "value" is outside the domain of "array" then nearest_index = 0
   or = size(array)-1 depending on whether array(0) or array(ia-1) is
   closest to "value"

   Arguments:
     value:  arbitrary data...same units as elements in "array"
     array:  array of data points  (must be monotonically increasing)
     ia   :  size of array.

 ********************************************************************/
int nearest_index(double value, const double *array, int ia)
{
  int index, i;
  int keep_going;

  for(i=1; i<ia; i++){
    if (array[i] < array[i-1]) 
      error_handler("nearest_index: array must be monotonically increasing"); 
  }
  if (value < array[0] )
    index = 0;
  else if ( value > array[ia-1]) 
    index = ia-1;
  else
    {
      i=0;
      keep_going = 1;
      while (i < ia && keep_going) {
	i = i+1;
	if (value <= array[i]) {
	  index = i;
	  if (array[i]-value > value-array[i-1]) index = i-1;
	  keep_going = 0;
	}
      }
    }
  return index;

};

/******************************************************************/

void tokenize(const char * const string, const char *tokens, unsigned int varlen,
	      unsigned int maxvar, char * pstring, unsigned int * const nstr)
{
  size_t i, j, nvar, len, ntoken;
  int found, n;
  
  nvar = 0; j = 0;
  len = strlen(string);
  ntoken = strlen(tokens);
  /* here we use the fact that C array [][] is contiguous in memory */
  if(string[0] == 0)error_handler("Error from tokenize: to-be-parsed string is empty");
  
  for(i = 0; i < len; i ++){
    if(string[i] != ' ' && string[i] != '\t'){
      found = 0;
      for(n=0; n<ntoken; n++) {
	if(string[i] == tokens[n] ) {
	  found = 1;
	  break;
	}
      }
      if(found) {
	if( j != 0) { /* remove :: */
	  *(pstring + (nvar++)*varlen + j) = 0;
	  j = 0;
	  if(nvar >= maxvar) error_handler("Error from tokenize: number of variables exceeds limit");
	}
      }
      else {
        *(pstring + nvar*varlen + j++) = string[i];
        if(j >= varlen ) error_handler("error from tokenize: variable name length exceeds limit during tokenization");
      }
    }
  }
  *(pstring + nvar*varlen + j) = 0;
  
  *nstr = ++nvar;

}

/*******************************************************************************
  double maxval_double(int size, double *data)
  get the maximum value of double array
*******************************************************************************/
double maxval_double(int size, const double *data)
{
  int n;
  double maxval;

  maxval = data[0];
  for(n=1; n<size; n++){
    if( data[n] > maxval ) maxval = data[n];
  }

  return maxval;
  
}; /* maxval_double */


/*******************************************************************************
  double minval_double(int size, double *data)
  get the minimum value of double array
*******************************************************************************/
double minval_double(int size, const double *data)
{
  int n;
  double minval;

  minval = data[0];
  for(n=1; n<size; n++){
    if( data[n] < minval ) minval = data[n];
  }

  return minval;
  
}; /* minval_double */

/*******************************************************************************
  double avgval_double(int size, double *data)
  get the average value of double array
*******************************************************************************/
double avgval_double(int size, const double *data)
{
  int n;
  double avgval;

  avgval = 0;
  for(n=0; n<size; n++) avgval += data[n];
  avgval /= size;
  
  return avgval;
  
}; /* avgval_double */


/*******************************************************************************
  void latlon2xyz
  Routine to map (lon, lat) to (x,y,z)
******************************************************************************/
void latlon2xyz(int size, const double *lon, const double *lat, double *x, double *y, double *z)
{
  int n;
  
  for(n=0; n<size; n++) {
    x[n] = cos(lat[n])*cos(lon[n]);
    y[n] = cos(lat[n])*sin(lon[n]);
    z[n] = sin(lat[n]);
  }

} /* latlon2xyz */

/*------------------------------------------------------------------------------
  double box_area(double ll_lon, double ll_lat, double ur_lon, double ur_lat)
  return the area of a lat-lon grid box. grid is in radians.
  ----------------------------------------------------------------------------*/
double box_area(double ll_lon, double ll_lat, double ur_lon, double ur_lat)
{
  double dx = ur_lon-ll_lon;
  double area;
  
  if(dx > M_PI)  dx = dx - 2.0*M_PI;
  if(dx < -M_PI) dx = dx + 2.0*M_PI;

  return (dx*(sin(ur_lat)-sin(ll_lat))*RADIUS*RADIUS ) ;
  
}; /* box_area */


/*------------------------------------------------------------------------------
  double box_area_unit_radius(double ll_lon, double ll_lat, double ur_lon, double ur_lat)
  return the area of a lat-lon grid box. grid is in radians. the area of the earth
  is 4*PI ( radius = 1)
  ----------------------------------------------------------------------------*/
double box_area_unit_radius(double ll_lon, double ll_lat, double ur_lon, double ur_lat)
{
  double dx = ur_lon-ll_lon;
  
  if(dx > M_PI)  dx = dx - 2.0*M_PI;
  if(dx < -M_PI) dx = dx + 2.0*M_PI;

  return (dx*(sin(ur_lat)-sin(ll_lat)) ) ;
}; /* box_area_unit_radius */


/*------------------------------------------------------------------------------
  double poly_area(const x[], const y[], int n)
  obtains area of input polygon by line integrating -sin(lat)d(lon)
  Vertex coordinates must be in degrees.
  Vertices must be listed counter-clockwise around polygon.
  grid is in radians.
  ----------------------------------------------------------------------------*/
double poly_area(const double x[], const double y[], int n)
{
  double area = 0.0;
  int    i;

  for (i=0;i<n;i++) {
    int ip = (i+1) % n;
    double dx = (x[ip]-x[i]);
    double lat1, lat2;
    if      (dx==0.0) continue;
    lat1 = y[ip];
    lat2 = y[i];    
    if(dx > M_PI)  dx = dx - 2.0*M_PI;
    if(dx < -M_PI) dx = dx + 2.0*M_PI;
    
    if (lat1 == lat2) /* cheap area calculation along latitude */
      area -= dx*sin(lat1);
    else
      area += dx*(cos(lat1)-cos(lat2))/(lat1-lat2);
  }
  return area*RADIUS*RADIUS;

}; /* poly_area */

/*------------------------------------------------------------------------------
  double poly_area_unit_radius(const x[], const y[], int n)
  obtains area of input polygon by line integrating -sin(lat)d(lon)
  Vertex coordinates must be in degrees.
  Vertices must be listed counter-clockwise around polygon.
  grid is in radians. the earth area is assumed to be 4*PI (radius=1).
  ----------------------------------------------------------------------------*/
double poly_area_unit_radius(const double x[], const double y[], int n)
{
  double area = 0.0;
  int    i;

  for (i=0;i<n;i++) {
    int ip = (i+1) % n;
    double dx = (x[ip]-x[i]);
    double lat1, lat2;
    if      (dx==0.0) continue;
    lat1 = y[ip];
    lat2 = y[i];    
    if(dx > M_PI)  dx = dx - 2.0*M_PI;
    if(dx < -M_PI) dx = dx + 2.0*M_PI;
    
    if (lat1 == lat2) /* cheap area calculation along latitude */
      area -= dx*sin(lat1);
    else
      area += dx*(cos(lat1)-cos(lat2))/(lat1-lat2);
  }
  
  return area;
}; /* poly_area_unit_radius */


int delete_vtx(double x[], double y[], int n, int n_del)
{
  for (;n_del<n-1;n_del++) {
    x[n_del] = x[n_del+1];
    y[n_del] = y[n_del+1];
  }
  
  return (n-1);
} /* delete_vtx */

int insert_vtx(double x[], double y[], int n, int n_ins, double lon_in, double lat_in)
{
  int i;

  for (i=n-1;i>=n_ins;i--) {
    x[i+1] = x[i];
    y[i+1] = y[i];
  }
  
  x[n_ins] = lon_in;
  y[n_ins] = lat_in;
  return (n+1);
} /* insert_vtx */

void v_print(double x[], double y[], int n)
{
  int i;

  for (i=0;i<n;i++) printf(" %20g   %20g\n", x[i], y[i]);
} /* v_print */

int fix_lon(double x[], double y[], int n, double tlon)
{
  double x_sum, dx;
  int i, nn = n, pole = 0;

  for (i=0;i<nn;i++) if (fabs(y[i])>=HPI-TOLORENCE) pole = 1;
  if (0&&pole) {
    printf("fixing pole cell\n");
    v_print(x, y, nn);
    printf("---------");
  }

  /* all pole points must be paired */
  for (i=0;i<nn;i++) if (fabs(y[i])>=HPI-TOLORENCE) {
    int im=(i+nn-1)%nn, ip=(i+1)%nn;

    if (y[im]==y[i] && y[ip]==y[i]) {
      nn = delete_vtx(x, y, nn, i);
      i--;
    } else if (y[im]!=y[i] && y[ip]!=y[i]) {
      nn = insert_vtx(x, y, nn, i, x[i], y[i]);
      i++;
    }
  }
  /* first of pole pair has longitude of previous vertex */
  /* second of pole pair has longitude of subsequent vertex */
  for (i=0;i<nn;i++) if (fabs(y[i])>=HPI-TOLORENCE) {
    int im=(i+nn-1)%nn, ip=(i+1)%nn;

    if (y[im]!=y[i]) x[i] = x[im];
    if (y[ip]!=y[i]) x[i] = x[ip];
  }

  if (nn) x_sum = x[0]; else return(0);
  for (i=1;i<nn;i++) {
    double dx = x[i]-x[i-1];

    if      (dx < -M_PI) dx = dx + TPI;
    else if (dx >  M_PI) dx = dx - TPI;
    x_sum += (x[i] = x[i-1] + dx);
  }

  dx = (x_sum/nn)-tlon;
  if      (dx < -M_PI) for (i=0;i<nn;i++) x[i] += TPI;
  else if (dx >  M_PI) for (i=0;i<nn;i++) x[i] -= TPI;

  if (0&&pole) {
    printf("area=%g\n", poly_area(x, y,nn));
    v_print(x, y, nn);
    printf("---------");
  }

  return (nn);
} /* fix_lon */
