////////////////////////////////////////////////////////////////////////////////
// File: adams_12_steps.c                                                     //
// Routines:                                                                  //
//    Adams_12_Steps                                                          //
//    Adams_Bashforth_12_Steps                                                //
//    Adams_Moulton_11_Steps                                                  //
//    Adams_12_Build_History                                                  //
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Description:                                                              //
//     The Adams-Bashforth method together with the Adams-Moulton method form //
//     a predictor-corrector pair for approximating the solution of the       //
//     differential equation y'(x) = f(x,y).                                  //
//     The general (k+1)-step Adams-Bashforth method is an explicit method    //
//     recursively given by:                                                  //
//       y[i+1] = y[i] + h * ( a[0]*f(x[i],y[i]) + a[1]*f(x[i-1],y[i-1])      //
//                             + ... + a[k]*f(x[i-k],y[i-k]) )                //
//     and the general k-step Adams_Moulton method is an implicit method      //
//     recursively given by:                                                  //
//       y[i+1] = y[i] + h * ( b[0]*f(x[i+1],y[i+1]) + b[1]*f(x[i],y[i])      //
//                             + ... + b[k]*f(x[i+1-k],y[i+1-k]) ).           //
//     For both the Adam's methods, x[i+1] - x[i] = h.                        //
//                                                                            //
//     The Adams-Moulton method, for sufficiently small h and using the       //
//     Adams-Bashforth estimate for y[i+1] as the initial estimate, is        //
//     contractive.  Therefore for sufficiently small h, the sequence of      //
//     estimates of y[i+1] generated by the Adams-Moulton method converges.   //
//                                                                            //
//     The truncation errors associated with both methods are of the order    //
//     h^(k+2).                                                               //
//                                                                            //
//     Note!  In order to approximate y[i+1], the (k+1)-step Adams-Bashforth  //
//     method requires that y[i-k], y[i-k+1], ... , y[i] are given, more      //
//     specifically, the method requires that f(x[i-k],y[i-k]), ... ,         //
//     f(x[i],y[i]) are given.  And in order to approximate y[i+1], the       //
//     k-step Adams-Moulton method requires that y[i-k+1], y[i-k+2],..., y[i] //
//     and an initial estimate of y[i+1] are given to start the iteration,    //
//     more specifically, the method requires that f(x[i-k],y[i-k]), ... ,    //
//     f(x[i],y[i]) are given as well as the initial estimate of y[i+1].      //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
#include <math.h>

static const double bashforth[] = { 4527766399.0, -19433810163.0,61633227185.0,
 -135579356757.0, 214139355366.0, -247741639374.0, 211103573298.0,
 -131365867290.0, 58189107627.0, -17410248271.0, 3158642445.0, -262747265.0 };

static const double moulton[] = {  262747265.0, 1374799219.0, -2092490673.0, 
     3828828885.0, -5519460582.0, 6043521486.0, -4963166514.0, 3007739418.0, 
                         -1305971115.0, 384709327.0, -68928781.0, 5675265.0 }; 

static const double divisor = 1.0 / 958003200.0;

#define STEPS sizeof(bashforth)/sizeof(bashforth[0])

double Adams_Bashforth_12_Steps( double y, double h, double f_history[] );
int Adams_Moulton_11_Steps( double (*f)(double, double), double y[], double x, 
             double h, double f_history[], double tolerance, int iterations );
static int hasConverged(double y0, double y1, double epsilon);

////////////////////////////////////////////////////////////////////////////////
// int Adams_12_Steps( double (*f)(double, double), double y[], double x0,    //
//      double h, double f_history[], double *y_bashforth,  double tolerance, //
//      int iterations )                                                      //
//                                                                            //
//  Description:                                                              //
//     This function approximates the solution of the differential equation   //
//     y' = f(x,y) at x0 + h using the starting values f(x0-i*h,y(x0-i*h)),   //
//     i = 1,...,11, stored in the array f_history[], and y(x0) stored in     //
//     y[0].                                                                  //
//                                                                            //
//  Arguments:                                                                //
//     double *f  Pointer to the function which returns the slope at (x,y) of //
//                integral curve of the differential equation y' = f(x,y)     //
//                which passes through the point (x0,y[0]).                   //
//     double y[] On input y[0] is the value of y at x0, on output y[1] is    //
//                the value at x0 + h.                                        //
//     double x0  The x value for y[0].                                       //
//     double h   Step size                                                   //
//     double f_history[]  On input, the previous values of f(x,y). i.e.      //
//                f_history[i] = f( x0-(11-i)*h, y(x0-(11-i)*h) ), i = 0,..., //
//                10.  On output, the updated history list,                   //
//                f_history[i] = f( x0-(12-i)*h, y(x0-(12-i)*h) ), i = 0,..., //
//                10.  On the initial call to this routine, f_history[i],     //
//                i = 0,..., 10, must be initialized by the calling routine.  //
//                Thereafter this function maintains the array.  The array    //
//                f_history[] must be dimensioned at least 12 in the calling  //
//                routine.  If the values y(x0-11*h) ,..., y(x0-h) are given, //
//                the user may call Adams_12_Build_History, given below, to   //
//                initialize the array f_history[].                           //
//     double *y_bashforth The predictor part, i.e. the Adams-Bashforth       //
//                estimate, of the predictor-corrector pair.                  //
//     double tolerance    The terminating tolerance for the corrector part   //
//                predictor-corrector pair.  This is not the error bounds for //
//                the solution y(x).                                          //
//     int    iterations   The maximum number of iterations for allow for the //
//                corrector to try to converge within the tolerance above.    //
//                                                                            //
//  Return Values:                                                            //
//     The number of iterations performed during the Adams-Moulton correction.//
//     The value of y(x) is stored in y[1].  If the return value is greater   //
//     than the user-specified iterations, the Adams-Moulton iteration failed //
//     to converge to the user-specified tolerance.                           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
int Adams_12_Steps( double (*f)(double, double), double y[], double x0, 
                  double h, double f_history[], double *y_bashforth,
                  double tolerance, int iterations ) {
   int i;
   int converged;

          // Calculate the predictor using the Adams-Bashforth formula 

   f_history[STEPS-1]  = (*f)(x0, y[0]);
   *y_bashforth  = Adams_Bashforth_12_Steps( y[0], h, f_history );
   for (i = 0; i < STEPS - 1; i++) f_history[i] = f_history[i+1];

          // Calculate the corrector using the Adams-Moulton formula 
   
   y[1] = *y_bashforth;
   return Adams_Moulton_11_Steps( f, y, x0+h, h, f_history, tolerance,
                                                               iterations );
}


////////////////////////////////////////////////////////////////////////////////
// double Adams_Bashforth_12_Steps( double y, double h, double f_history[] )  //
//                                                                            //
//  Description:                                                              //
//     This function uses the Adams-Bashforth method to approximate the solu- //
//     tion of the differential equation y' = f(x,y) at x0 + h, where x0 is   //
//     the argument of y(x) where the input value y = y(x0).   This method    //
//     uses the starting values f(x0-i*h,y(x0-i*h)), i = 0,..., 11, stored in //
//     the array f_history[] and the input argument y = y(x0).                //
//                                                                            //
//  Arguments:                                                                //
//     double y   The value of y at x0, the return value is y(x0+h).          //
//     double h   Step size                                                   //
//     double f_history[]  The previous values of f(x,y). i.e.                //
//                f_history[i] = f( x0-(11-i)*h, y(x0-(11-i)*h) ),            //
//                i = 0,..., 11.  The array f_history[] must be dimensioned   //
//                at least 12 in the calling routine.                         //
//                                                                            //
//  Return Values:                                                            //
//     y(x0+h) where y(x0) was the input argument for y.                      //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
double Adams_Bashforth_12_Steps( double y, double h, double f_history[] ) {

   double delta = 0.0;
   int i;
   int n = STEPS - 1;

          // Calculate the predictor using the Adams-Bashforth formula 

   for (i = 0; i < STEPS; i++, n--) delta += bashforth[i] * f_history[n];

   return y + h * divisor * delta;
}


////////////////////////////////////////////////////////////////////////////////
// int Adams_Moulton_11_Steps( double (*f)(double, double), double y[],       //
// double x, double h, double f_history[], double tolerance, int iterations ) //
//                                                                            //
//  Description:                                                              //
//     This function uses the Adams-Moulton method to iterate for an estimate //
//     of the solution of the differential equation y' = f(x,y) at (x,y[1])   //
//     using starting values f(x-i*h,y(x-i*h)), i = 1,...,11, stored in the   //
//     array f_history[], the value of y(x-h) stored in y[0] and the initial  //
//     estimate of y(x) stored in y[1].                                       //
//                                                                            //
//  Arguments:                                                                //
//     double *f  Pointer to the function which returns the slope at (x,y) of //
//                integral curve of the differential equation y' = f(x,y)     //
//                which passes through the point (x-h,y[0]).                  //
//     double y[] On input y[1] is the prediction of y at x, on output y[1]   //
//                is the corrected value of y at x.                           //
//     double x   The x value for y[1].                                       //
//     double h   Step size                                                   //
//     double f_history[]  The previous values of f(x,y). i.e. f_history[0] = //
//                f_history[i] = f( x-(11-i)*h, y(x-(11-i)*h) ), i = 0,...,   //
//                10.  The array f_history[] must be dimensioned at least 11  //
//                in the calling routine.                                     //
//     double tolerance    The terminating tolerance for the corrector part   //
//                predictor-corrector pair.  This is not the error bounds for //
//                the solution y(x).                                          //
//     int    iterations   The maximum number of iterations for allow for the //
//                corrector to try to converge within the tolerance above.    //
//                                                                            //
//  Return Values:                                                            //
//     The number of iterations performed.  The value of y(x) is stored in    //
//     y[1].                                                                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
int Adams_Moulton_11_Steps( double (*f)(double, double), double y[], double x, 
             double h, double f_history[], double tolerance, int iterations ) {

   double old_estimate;
   double delta = 0.0;
   int i;
   int n = STEPS - 2;
   int converged = -1;

          // Calculate the corrector using the Adams-Moulton formula 
   
   for (i = 1; i < STEPS; i++, n--) delta += moulton[i] * f_history[n];

   for (i = 0; i < iterations; i++) {
      old_estimate = y[1];
      y[1] = y[0] + h * divisor * ( moulton[0] * (*f)(x, y[1]) + delta );
      if ( hasConverged(old_estimate, y[1], tolerance) ) break;
   }
   return i+1;
}


////////////////////////////////////////////////////////////////////////////////
// void Adams_12_Build_History(double (*f)(double,double), double f_history[],//
//                                            double y[], double x, double h) //
//                                                                            //
//  Description:                                                              //
//     This function saves the historical values of f(x,y) in order to begin  //
//     the Adams-Bashforth and Adams-Moulton recursions.  The historical      //
//     values are saved in the array f_history[].  If on input, the values    //
//     y[i] = y(x + i*h) for i = 0,..., 11 are given.  Then                   //
//     f_history[i] = f(x+i*h,y[i]).                                          //
//                                                                            //
//  Arguments:                                                                //
//     double *f  Pointer to the function which returns the slope at (x,y) of //
//                integral curve of the differential equation y' = f(x,y)     //
//                which passes through the point (x,y[0]).                    //
//     double f_history[]  The previous values of f(x,y). i.e. f_history[0] = //
//                f(x,y[0]), f_history[1] = f(x+h, y[1]), ... ,               //
//                f_history[11] = f(x+11*h,y[11]).                            //
//                The array f_history must be dimensioned at least 12.        //
//     double y[] On input y[i] is the value of y at x + i*h.                 //
//     double x   The x value for y[0].                                       //
//     double h   Step size                                                   //
//                                                                            //
//  Return Values:                                                            //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
void Adams_12_Build_History(double (*f)(double,double), double f_history[],
                                              double y[], double x, double h) {
   
   int i;

   for (i = 0; i < STEPS-1; x +=h, i++) f_history[i] = (*f)(x,y[i]);
}


////////////////////////////////////////////////////////////////////////////////
// int hasConverged(double y0, double y1, double epsilon)                     //
//                                                                            //
//  Description:                                                              //
//     This function returns 1 (true) if the relative difference of y0 and    //
//     y1 is less than epsilon when | y0 | and | y1 | are greater than 1 or   //
//     if the absolute difference of y0 and y1 is less than epsilon when      //
//     | y0 | or | y1 | is less than 1.  If neither condition is true the     //
//     function returns 0 (false).                                            //
//                                                                            //
//  Arguments:                                                                //
//     double y0  The previous estimate of y(x) using the Adams-Moulton       //
//                corrector.                                                  //
//     double y1  The current estimate of y(x) using the Adams-Moulton        //
//                corrector.                                                  //
//     double epsilon  Relative tolerance if min( |y0|, |y1| ) > 1 and        //
//                absolute tolerance if min( |y0|, |y1| ) <= 1.               //
//                                                                            //
//  Return Values:                                                            //
//     1:  if min( |y0|, |y1| ) > 1 and | y0 - y1 | < | y1 | * epsilon or     //
//         if min( |y0|, |y1| ) <= 1 and | y0 - y1 | < epsilon.               //
//     0:  Otherwise.                                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
static int hasConverged(double y0, double y1, double epsilon) {

   if ( fabs(y0) > 1.0 && fabs(y1) > 1.0 ) epsilon *= fabs(y1);
   if ( fabs(y0 - y1) < epsilon ) return 1;
   return 0;
}