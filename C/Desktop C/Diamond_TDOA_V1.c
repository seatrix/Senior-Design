/*
*	Author:  Joshua Simmons, Zack Goyetche, Michael Daub, and Shane Stroh
*	Started: September, 2015
*	Status:  INCOMPLETE
*
*	PINGER ASSUMED TO BE INTERMITTENT AT FIXED INTERVALS!!!
*
*	Description: Uses Time-Difference-of-Arrival (TDOA) to determine the azimuth to
*				 pulsing signal source underwater.
*
*	Source Characteristics (Teledyne Benthos)
*		1. Waveform: sinusoidal
*		2. Frequency: 25-40 [kHz] in 0.5 [kHz] increments
*		3. Pulse Length: 4.0 [ms]
*		4. Pulse Repetition: 0.5 OR 1 OR 2 [s]
*       	5. Acoustic Output Power: 0.125-5.000 [W]
*       	6. Depth Rating: 750 [m]
*
*	3D Cartesian co-ordinate system with the origin centered in the middle of the sensor
*	array. Sensor geometry is square shaped residing all in the XY-plane.
*
*	Sensor layout
*
*		  (Top View)
*	---------------------
*	|                   |
*	|         1         |
*	|                   |
*	|     4       2     |
*	|                   |
*	|         3         |
*	|                   |
*	---------------------
*
*	Coordinates
*		chan1  = ( 0, d,0)		D = sqrt(2)*d
*		chan2  = ( d, 0,0)
*		chan3  = ( 0,-d,0)
*		chan4  = (-d, 0,0)
*		pinger = (xP,yP,zP)
*
*	Sequence of Events
*		1. Initialization of parameters
*		2. Synchorization with pinger using a single channel
*			A. Chan1 sampled
*			B. FFT
*			C. Power
*			    i. Adjust PGA gain if necessary
*			D. Ideal bandpass filter
*			E. iFFT
*			F. Pulse-Repetitive-Period estimated.
*				i. Adjust trigger delay
*			G. Heads centered in frame window.
*				i. Adjust trigger delay
*		3. All 4 channels sampled and azimuth to source estimated
*			A. All channels sampled
*			B. FFT
*			C. Power
*			    i. Adjust PGA gain if necessary
*			D. Ideal bandpass filter
*			E. iFFT
*			F. Break TOAs
*				i. Head triggered left-to-right break threshold
*			G. Break time delays
*				i. Difference in secondary pseudo-TOAs
*			H. XC time delays
*				i. Cross-correlation sliver
*			I. Comparing break and XC time delays
*			J. Primary TOA
*			K. Sphere radii
*			L. Pseudo pinger location
*			M. Azimuths
*				i.   Checking for complex solutions
*				ii.  Saving last 10 azimuths
*				iii. Taking median of last 10 azimuths
*
*   Programming Conventions:
*       1. All arrays have their size stored in their zeroth element.
*           Example: int myArray[1+10] = {10,1,2,3,4,5,6,7,8,9,10}; 
*/

/////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// PREPROCESSORS //////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

#include "js_cmath.h"
#include "js_tdoa.h"

/////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// PIN DECLARATIONS /////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

// NONE!

/////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// CONSTANT DECLARATIONS //////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

// Pinger Parameters
static const double fPinger = 37.0E+3;// Pinger Frequency [Hz]

// Hydrophone Parameters
static const double vP = 1482.0;            // Velocity of Propagation [m/s]
static const double lambda = vP/fPinger;    // Wavelength [m]
static const double D = lambda;             // Sensor spacing [m]
static const double d = D/1.414213562373095;// For System of Coordinates [m]

// ADC Parameters
static const double fADC = 1800.0E+3;// ADC Sampling Frequency [Hz]
static const double tADC = 1.0/fADC; // ADC Sampling Period [s]
static const int N0 = 1024;          // Frame Size [samples]

// FFT Parameters
static const double T0 = N0*tADC;// Truncation Time Interval [s]
static const double f0 = 1.0/T0; // Frequency Resolution [Hz]

// Ideal Digital Bandpass Filter Parameters
static const double fCenter = 30.0E+3;// Center frequency [Hz]
static const double halfChan = 5.0E+3;// Channel half width [Hz]

// Processor Parameters
static const int iBound = (int) D/(vP*tADC)+1;     // Maximum +/- Index for XCs
static const int medianSize = 10;                  // For Taking Medians of Azimuths
static const int peakCounterMax = (int) D/lambda+1;// Maximum Number of Peaks for XCs
static const double THD = 0.9;			   // Threshold Level

/////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// VARIABLE DECLARATIONS //////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

// Azimuths
double azimuthH, azimuthV1, azimuthV2;
double azimuthHArray[1+medianSize] = {medianSize};
double azimuthV1Array[1+medianSize] = {medianSize};
double azimuthV2Array[1+medianSize] = {medianSize};

// Cross-Correlations
double XC1x[1+(2*iBound+1)] = {2*iBound+1};

// Frequency Sampled Data
double complex chanx_f[1+N0] = {N0};

// Frequency Vector (double-sided)
double f[1+N0] = {N0};

// Ideal Digital Bandpass Filter
double complex H[1+N0] = {N0};

// Pinger
double pingerCoordinates[1+3] = {3};
int pingerSynced = 0;
double PRT = 2.0;

// Power
double powerx_f;

// Time Sampled Data
double chanx_t[4][1+N0];
    chanx_t[0][0] = N0;
    chanx_t[1][0] = N0;
    chanx_t[2][0] = N0;
    chanx_t[3][0] = N0;

// Time Vector
double t[1+N0] = {N0};

// TDOA Algorithm
double sphereRadii[1+4] = {4};
double tD2, tD3, tD4, TOA;
double tD_BreakWall[1+4] = {4};
double tD_XC[1+maxPeaks] = {maxPeaks};
double TOA_BreakWall[1+4] = {4};

/////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// START MAIN ROUTINE ///////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

int main (void){

    // Constructing Frequency Vector (double-sided)
    for (int i = 1; i <= f[0]; i++) { f[i] = f0*(-N0/2+(i-1)); }

    // Constructing Ideal Digital Bandpass Filter
    for (int i = 1; i <= H[0]; i++) {
        if ( abs(f[i]) >= fCent-halfChan && abs(f[i]) <= fCent+halfChan )
            { H[i] = 1.0; }
        else
            { H[i] = 0.0; }
    }

    // Constructing Time Vector
    for (int i = 1; i <= t[0]; i++) { t[i] = i*tADC; }

/*
    while (1) {

        if ( pingerSynced == 0 ) {
            //Sample chan1
            FFT();
            Power();
            AdjustPGA();
            BPF();
            iFFT();
            syncPinger();
            Delay(PRT);
        }

        else if ( pingerSynced == 1 ) {
            //Sample all channels
            FFT();
            Power();
            AdjustPGA();
            BPF();
            iFFT();
            BreakWall_TOAs(); // Re-sync Pinger if -1 returned
            BreakWall_tDs();
            XC_tDs();
            Compare_tDs();
            Superior_TOA();
            SphereRadii();
            PingerLocation();
            PingerAzimuth();
            Delay(PRT);
        }

        else;
    }
*/
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// END OF PROGRAM ////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

