#ifndef JS_TDOA_H
#define JS_TDOA_H

double complex BPF (double complex Y[], int yLength, double H[]);
//BreakWall_tDs();
//BreakWall_TOAs();
//Compare_tDs();
void PingerLocation (double PingerLoc[], double R[], double d);
//PingerAzimuth();
void SphereRadii (double R[], double tD2, double tD3, double tD4, double TOA, double vP);
//Superior_TOA();
//syncPinger();
//XC_tDs();

#endif
