/*********************************************************
 * config.h - Configuration data for the driver.c program.
 *********************************************************/
#ifndef _CONFIG_H_
#define _CONFIG_H_

/*
 * CPEs for the baseline (naive) version of the rotate function that
 * was handed out to the students. Rd is the measured CPE for a dxd
 * image. Run the driver.c program on your system to get these
 * numbers.
 */
#define R64    2.1
#define R128   3.0
#define R256   5.2
#define R512   8.7
#define R1024  13.1

/*
 * CPEs for the baseline (naive) version of the smooth function that
 * was handed out to the students. Sd is the measure CPE for a dxd
 * image. Run the driver.c program on your system to get these
 * numbers.
 */
#define S32   58.8
#define S64   59.6
#define S128  61.7
#define S256  61.9
#define S512  62.0

#endif /* _CONFIG_H_ */
