/*! @file Driver file for LX200 telescope
 * $Id$
 * Based on original c library for xephem writen by Ken Shouse
 * <ken@kshouse.engine.swri.edu> and modified by Carlos Guirao
 * <cguirao@eso.org>
 *
 * This is JUST A DRIVER, you shall to consult deamon for network
 * interface.
 *
 * Uses semaphores to control file acces.
 *
 * @author petr 
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>

#include <sys/ipc.h>
#include <sys/sem.h>

#include "lx200.h"
#include "../utils/hms.h"
#include "../status.h"

//! device name
char *port_dev;
//! port descriptor
int port;

//! sempahore id structure, we have two semaphores...
int semid;

//! port timeout
#define PORT_TIME_OUT 5;

//! one semaphore for direct port controll..
#define SEM_TEL 	0
//! and second for controll of the move operation
#define SEM_MOVE 	1

#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
/* union semun is defined by including <sys/sem.h> */
#else
/* according to X/OPEN we have to define it ourselves */
union semun
{
  int val;			/* value for SETVAL */
  struct semid_ds *buf;		/* buffer for IPC_STAT, IPC_SET */
  unsigned short int *array;	/* array for GETALL, SETALL */
  struct seminfo *__buf;	/* buffer for IPC_INFO */
};
#endif

void
tel_cleanup ()
{
  if (semctl (semid, 1, IPC_RMID) < 0)
    syslog (LOG_ERR, "semctl %i IPC_RMID: %m", semid);
}

/*!
 * Reports telescope status.
 *
 * @param status		status to return
 * 
 * @return 0 on success, -1 & set errno otherwise
 */
int
tel_status (int *status)
{
  union semun sem_un;
  *status = 0;
  if (semctl (semid, SEM_MOVE, IPC_STAT, &sem_un) < 0)
    {
      syslog (LOG_ERR, "semctl: %m");
      return -1;
    }
  *status |= sem_un.val ? TEL_MOVING : TEL_STILL;
  return 0;
}

/*!
 * Connect on given port.
 * 
 * @param devptr Pointer to device name
 * 
 * @return 0 on succes, -1 & set errno otherwise
 */
int
tel_connect (const char *devptr)
{
  struct termios tel_termios;
  union semun sem_un;
  unsigned short int sem_arr[] = { 1, 1 };

  port_dev = malloc (strlen (devptr) + 1);
  strcpy (port_dev, devptr);
  port = open (port_dev, O_RDWR);
  free (port_dev);
  if (port < 0)
    return -1;

  if (tcgetattr (port, &tel_termios) < 0)
    return -1;

  if (cfsetospeed (&tel_termios, B9600) < 0 ||
      cfsetispeed (&tel_termios, B9600) < 0)
    return -1;
  tel_termios.c_iflag = IGNBRK & ~(IXON | IXOFF | IXANY);
  tel_termios.c_oflag = 0;
  tel_termios.c_cflag =
    ((tel_termios.c_cflag & ~(CSIZE)) | CS8) & ~(PARENB | PARODD);
  tel_termios.c_lflag = 0;
  tel_termios.c_cc[VMIN] = 1;
  tel_termios.c_cc[VTIME] = 5;

  if (tcsetattr (port, TCSANOW, &tel_termios) < 0)
    {
      syslog (LOG_ERR, "tcsetattr: %m");
      return -1;
    }

  if ((semid = semget (ftok (devptr, 0), 2, IPC_CREAT | 0644)) < 0)
    {
      syslog (LOG_ERR, "semget: %m");
      return -1;
    }

  sem_un.array = sem_arr;

  if (semctl (semid, 0, SETALL, sem_un) < 0)
    {
      syslog (LOG_ERR, "semctl init: %m");
      return -1;
    }

  syslog (LOG_DEBUG, "LX200:Initialization complete");

  return tcflush (port, TCIOFLUSH);
}

/*! 
 * Reads some data directly from port.
 * 
 * Log all flow as LOG_DEBUG to syslog
 * 
 * @exception EIO when there aren't data from port
 * @exception common select() exceptions
 * 
 * @param buf 		buffer to read in data
 * @param count 	how much data will be readed
 * 
 * @return -1 on failure, otherwise number of read data 
 */
int
tel_read (char *buf, int count)
{
  int readed;
  fd_set pfds;
  struct timeval tv;

  tv.tv_sec = PORT_TIME_OUT;
  tv.tv_usec = 0;

  for (readed = 0; readed < count; readed++)
    {
      FD_ZERO (&pfds);
      FD_SET (port, &pfds);
      if (select (port + 1, &pfds, NULL, NULL, &tv) == 0)
	{
	  syslog (LOG_DEBUG, "LX200: port timeout %m");
	  return -1;
	}

      if (FD_ISSET (port, &pfds))
	{
	  if (read (port, &buf[readed], 1) < 0)
	    {
	      syslog (LOG_DEBUG, "LX200: port read error %m");
	      return -1;
	    }
	  syslog (LOG_DEBUG, "readed: %c", buf[readed]);
	}
      else
	{
	  syslog (LOG_DEBUG, "LX200:Cannot retrieve data from port");
	  errno = EIO;
	  return -1;
	}
    }
  return readed;
}

/*! 
 * Will read from port till it encoutered # character.
 * 
 * Read ending #, but doesn't return it.
 * 
 * @see tel_read() for description
 */
int
tel_read_hash (char *buf, int count)
{
  int readed;
  buf[0] = 0;

  for (readed = 0; readed < count; readed++)
    {
      if (tel_read (&buf[readed], 1) < 0)
	return -1;
      if (buf[readed] == '#')
	break;
    }
  if (buf[readed] == '#')
    buf[readed] = 0;
  syslog (LOG_DEBUG, "LX200:Hash-readed:'%s'", buf);
  return readed;
}

/*! 
 * Will write on telescope port string.
 *
 * @exception EIO, .. common write exceptions 
 * 
 * @param buf 		buffer to write
 * @param count 	count to write
 * 
 * @return -1 on failure, count otherwise
 */

int
tel_write (char *buf, int count)
{
  syslog (LOG_DEBUG, "LX200:will write:'%s'", buf);
  return write (port, buf, count);
}


/*! 
 * Combine write && read together.
 * 
 * Flush port to clear any gargabe.
 *
 * @exception EINVAL and other exceptions
 * 
 * @param wbuf		buffer to write on port
 * @param wcount	write count
 * @param rbuf		buffer to read from port
 * @param rcount	maximal number of characters to read
 * 
 * @return -1 and set errno on failure, rcount otherwise
 */

int
tel_write_read (char *wbuf, int wcount, char *rbuf, int rcount)
{
  int tmp_rcount = -1;
  struct sembuf sem_buf;

  sem_buf.sem_num = SEM_TEL;
  sem_buf.sem_op = -1;
  sem_buf.sem_flg = SEM_UNDO;

  if (semop (semid, &sem_buf, 1) < 0)
    return -1;
  if (tcflush (port, TCIOFLUSH) < 0)
    goto unlock;		// we need to unlock
  if (tel_write (wbuf, wcount) < 0)
    goto unlock;

  syslog (LOG_DEBUG, "LX200:will read %i", rcount);
  tmp_rcount = tel_read (rbuf, rcount);

  syslog (LOG_DEBUG, "LX200:will read %i %m", tmp_rcount);

unlock:

  sem_buf.sem_op = 1;

  if (semop (semid, &sem_buf, 1) < 0)
    {
      syslog (LOG_EMERG, "LX200:Cannot perform semop in tel_write_read:%m");
      return -1;
    }

  return tmp_rcount;
}

/*! 
 * Combine write && read_hash together.
 *
 * @see tel_write_read for definition
 */
int
tel_write_read_hash (char *wbuf, int wcount, char *rbuf, int rcount)
{
  int tmp_rcount = -1;
  struct sembuf sem_buf;

  sem_buf.sem_num = SEM_TEL;
  sem_buf.sem_op = -1;
  sem_buf.sem_flg = SEM_UNDO;

  if (semop (semid, &sem_buf, 1) < 0)
    return -1;
  if (tcflush (port, TCIOFLUSH) < 0)
    goto unlock;		// we need to unlock
  if (tel_write (wbuf, wcount) < 0)
    goto unlock;

  tmp_rcount = tel_read_hash (rbuf, rcount);

unlock:

  sem_buf.sem_op = 1;

  if (semop (semid, &sem_buf, 1) < 0)
    {
      syslog (LOG_EMERG,
	      "LX200:Cannot perform semop in tel_write_read_hash:%m");
      return -1;
    }

  return tmp_rcount;
}

/*! 
 * Set slew rate. For completness?
 * 
 * This functions are there IMHO mainly for historical reasons. They
 * don't have any use, since if you start move and then die, telescope
 * will move forewer till it doesn't hurt itself. So it's quite
 * dangerous to use them for automatic observation. Better use quide
 * commands from attached CCD, since it defines timeout, which rules
 * CCD.
 *
 * @param new_rate	new rate to set. Uses RATE_<SPEED> constant.
 * 
 * @return -1 on failure & set errno, 5 (>=0) otherwise
 */
int
tel_set_rate (char new_rate)
{
  char command[6];
  sprintf (command, "#:R%c#", new_rate);
  return tel_write (command, 5);
}

/*! 
 * Start slew. Again for completness?
 * 
 * @see tel_set_rate() for speed.
 *
 * @param direction 	direction
 * 
 * @return -1 on failure & set errnom, 5 (>=0) otherwise
 */
int
tel_start_slew (char direction)
{
  char command[6];
  sprintf (command, "#:M%c#", direction);
  return tel_write (command, 5) == 1 ? -1 : 0;
}

/*! 
 * Stop sleew. Again only for completness?
 * 
 * @see tel_start_slew for direction.	
 */
int
tel_stop_slew (char direction)
{
  char command[6];
  sprintf (command, "#:Q%c#", direction);
  return tel_write (command, 5) < 0 ? -1 : 0;
}

/*!
 * Stop telescope slewing at any direction
 *
 * @return 0 on success, -1 and set errno on error
 */
int
tel_stop_slew_any ()
{
  char dirs[] = { 'e', 'w', 'n', 's' };
  int i;
  for (i = 0; i < 4; i++)
    {
      if (tel_stop_slew (dirs[i]) < 0)
	return -1;
    }
  return 0;
}


/*!
 * Disconnect lx200.
 *
 * @return -1 on failure and set errno, 0 otherwise
 */
int
tel_disconnect (void)
{
  return close (port);
}

/*! 
 * Reads some value from lx200 in hms format.
 * 
 * Utility function for all those read_ra and other.
 * 
 * @param hmsptr	where hms will be stored
 * 
 * @return -1 and set errno on error, otherwise 0
 */
int
tel_read_hms (double *hmsptr, char *command)
{
  char wbuf[11];
  if (tel_write_read_hash (command, strlen (command), wbuf, 10) < 0)
    return -1;
  *hmsptr = hmstod (wbuf);
  if (errno)
    return -1;
  return 0;
}

/*! 
 * Reads lx200 right ascenation.
 * 
 * @param raptr		where ra will be stored
 * 
 * @return -1 and set errno on error, otherwise 0
 */
int
tel_read_ra (double *raptr)
{
  return tel_read_hms (raptr, "#:GR#");
}

/*!
 * Reads LX200 declination.
 * 
 * @param decptr	where dec will be stored
 * 
 * @return -1 and set errno on error, otherwise 0
 */
int
tel_read_dec (double *decptr)
{
  return tel_read_hms (decptr, "#:GD#");
}

/*! 
 * Returns LX200 local time.
 * 
 * @param tptr		where time will be stored
 * 
 * @return -1 and errno on error, otherwise 0
 */
int
tel_read_localtime (double *tptr)
{
  return tel_read_hms (tptr, "#:GL#");
}

/*! 
 * Returns LX200 sideral time.
 * 
 * @param tptr		where time will be stored
 * 
 * @return -1 and errno on error, otherwise 0
 */
int
tel_read_siderealtime (double *tptr)
{
  return tel_read_hms (tptr, "#:GS#");
}

/*! 
 * Reads LX200 latitude.
 * 
 * @param latptr	here latitude will be stored  
 * 
 * @return -1 and errno on error, otherwise 0
 */
int
tel_read_latitude (double *tptr)
{
  return tel_read_hms (tptr, "#:Gt#");
}

/*! 
 * Reads LX200 longtitude.
 * 
 * @param latptr	where longtitude will be stored  
 * 
 * @return -1 and errno on error, otherwise 0
 */
int
tel_read_longtitude (double *tptr)
{
  return tel_read_hms (tptr, "#:Gg#");
}

/*! 
 * Repeat LX200 write.
 * 
 * Handy for setting ra and dec.
 * Meade tends to have problems with that.
 *
 * @param command	command to write on port
 */
int
tel_rep_write (char *command)
{
  int count;
  char retstr;
  for (count = 0; count < 200; count++)
    {
      if (tel_write_read (command, strlen (command), &retstr, 1) < 0)
	return -1;
      if (retstr == '1')
	break;
      sleep (1);
      syslog (LOG_DEBUG, "LX200:tel_rep_write - for %i time.", count);
    }
  if (count == 200)
    {
      syslog (LOG_ERR,
	      "LX200:tel_rep_write unsucessful due to incorrect return.");
      errno = EIO;
      return -1;
    }
  return 0;
}

/*! 
 * Set LX200 right ascenation.
 *
 * @param ra		right ascenation to set in decimal hours
 *
 * @return -1 and errno on error, otherwise 0
 */
int
tel_write_ra (double ra)
{
  char command[14];
  int h, m, s;
  if (ra < 0)
    ra = floor (ra / 24) * -24 + ra;	//normalize ra
  if (ra > 24)
    ra = ra - floor (ra / 24) * 24;
  dtoints (ra, &h, &m, &s);
  if (snprintf (command, 14, "#:Sr%02d:%02d:%02d#", h, m, s) < 0)
    return -1;
  return tel_rep_write (command);
}

/*! 
 * Set LX200 declination.
 *
 * @param dec		declination to set in decimal degrees
 * 
 * @return -1 and errno on error, otherwise 0
 */
int
tel_write_dec (double dec)
{
  char command[15];
  int h, m, s;
  if (dec < 0)
    dec = floor (dec / 90) * -90 + dec;	//normalize dec
  if (dec > 90)
    dec = dec - floor (dec / 90) * 90;
  dtoints (dec, &h, &m, &s);
  if (snprintf (command, 15, "#:Sd%+02d\xdf%02d:%02d#", h, m, s) < 0)
    return -1;
  return tel_rep_write (command);
}

/*! 
 * Slew (=set) LX200 to new coordinates.
 *
 * @param ra 		new right ascenation
 * @param dec 		new declination
 * @exception EINVAL 	when telescope is below horizont, or upper limit was reached
 *
 * @return -1 and errno on exception, otherwise 0
 */
int
tel_slew_to (double ra, double dec)
{
  char retstr;
  int max_read = 200;		// maximal read till # is encountered
  if (tel_write_ra (ra) < 0 || tel_write_dec (dec) < 0)
    return -1;
  if (tel_write_read ("#:MS#", 5, &retstr, 1) < 0)
    return -1;
  if (retstr == '0')
    return 0;
  retstr = 0;
  while (max_read > 0 && retstr != '#')
    {
      if (tel_read (&retstr, 1) < 0)
	return -1;
      max_read--;
    }
  errno = EINVAL;
  return -1;
}

/*! 
 * Check, if telescope match given coordinates.
 *
 * @param ra		target right ascenation
 * @param dec		target declination
 *
 * @return -1 and errno on exception, 0 if matched, 1 if not matched
 */
int
tel_check_coords (double ra, double dec)
{
  double err_ra, err_dec;
  if ((tel_read_ra (&err_ra) < 0) || (tel_read_dec (&err_dec) < 0))
    return -1;
  err_ra -= ra;
  err_dec -= dec;
  if (fabs (err_ra) > 0.05 || fabs (err_dec) > 0.5)
    return 1;
  return 0;
}

/*! 
 * Move telescope to new location, wait for completition, then send
 * message.
 *
 * @param ra		target right ascenation
 * @param dec		target declination
 * 
 * @return -1 and errno on error, 0 otherwise
 * 
 * @execption ETIMEDOUT when timeout occurs
 */
int
tel_move_to (double ra, double dec)
{
  int timeout;
  int ret;
  struct sembuf sem_buf;

  sem_buf.sem_num = SEM_MOVE;
  sem_buf.sem_op = -1;
  sem_buf.sem_flg = SEM_UNDO;

  syslog (LOG_INFO, "LX200:tel move_to ra:%f dec:%f", ra, dec);

  if (semop (semid, &sem_buf, 1) < 0)
    return -1;

  sem_buf.sem_op = 1;

  if ((ret = tel_slew_to (ra, dec)) < 0)
    {
      goto unlock;
    }
  timeout = time (NULL) + 100;	// it's quite reasonably to hope that call will not fail
  while ((time (NULL) <= timeout) && (!tel_check_coords (ra, dec)))
    sleep (2);
  // wait for a bit, telescope get confused?? when you check for ra 
  // and dec often and sometime doesn't move to required location
  // this is still just a theory, which waits for final proof

  if (time (NULL) > timeout)
    {
      syslog (LOG_ERR, "LX200:Timeout during moving to ra:%f dec:%f.", ra, dec);	// mayby will be handy to add call to tel_get_ra+dec
      // to obtain current ra and dec
      if (semop (semid, &sem_buf, 1) < 0)
	{
	  syslog (LOG_EMERG, "LX200:Cannot perform semop in tel_move_to:%m");
	  return -1;
	}
      errno = ETIMEDOUT;
      return -1;
    }
  sleep (5);
  // need to sleep a while, waiting for final precise adjustement 
  // of the scope, which could be checked by checkcoords - the scope 
  // newer gets to precise position, it just get to something about 
  // that, so we could program checkcoords to check for precise position.  
  // Also good to wait till the things settle down.
unlock:

  if (semop (semid, &sem_buf, 1) < 0)
    {
      syslog (LOG_EMERG, "LX200:Cannot perform semop in tel_move_to:%m");
      return -1;
    }

  syslog (LOG_DEBUG, "LX200:tel_move_to ra:%f dec:%f returns %i", ra, dec,
	  ret);
  return ret;
}

/*! 
 * Repeat move_to 5 times, with a bit changed location.
 *
 * Used to uvercome LX200 error.
 *
 * @see tel_move_to
 */
int
tel_rep_move_to (double ra, double dec)
{
  int i;
  for (i = 0; i < 5; i++)
    if (!tel_move_to (ra, dec))
      {
	return 0;		// we finished move without error
      }
    else
      {
	if (errno != ETIMEDOUT)
	  {
	    return -1;		// some error occured
	  }
	// when we have ETIMEDOUT, let's try again.
	ra = ra + i * 0.05;	// magic value, most often helps
      }
  // when we got so far, something realy wrong must happen
  syslog (LOG_EMERG, "LX200:Too much timeouts during moving to ra:%f dec:%f",
	  ra, dec);
  return -1;
}

/*! 
 * Set telescope to match given coordinates
 *
 * This function is mainly used to tell the telescope, where it
 * actually is at the beggining of observation (remember, that lx200
 * doesn't have absolute position sensors)
 * 
 * @param ra		setting right ascennation
 * @param dec		setting declination
 * 
 * @return -1 and set errno on error, otherwise 0
 */
int
tel_set_to (double ra, double dec)
{
  char readback[101];
  if ((tel_write_ra (ra) < 0) || (tel_write_dec (dec) < 0))
    return -1;
  if (tel_write_read_hash ("#:CM#", 5, readback, 100) < 0)
    return -1;
  // since we are carring operation critical for next movements of telescope, 
  // we are obliged to check its correctness 
  return tel_check_coords (ra, dec);
}

/*! 
 * Initialize telescope.
 * 
 * send some special commands to make initialization
 * 
 * @return -1 and set errno on error, otherwise 0
 */
int
tel_init ()
{
  char rbuf[10];
  // we get 12:34:4# while we're in short mode
  // and 12:34:45 while we're in long mode
  if (tel_write_read ("#:Gr#", 5, rbuf, 8) < 0)
    return -1;
  if (rbuf[7] == '#')
    {				// that could be used to distinguish between long
      // short mode
      // we are in short mode, set the long on
      if (tel_write_read ("#:U#", 5, rbuf, 0) < 0)
	return -1;
      // now set low precision, e.g. we won't wait for user
      // to find a star
      strcpy (rbuf, "HIGH");
      while (strncmp ("HIGH", rbuf, 4))
	{
	  if (tel_write_read ("#:P#", 4, rbuf, 4) < 0)
	    return -1;
	}
    }
  return 0;
}

/*!
 * Test, if telescope is connected to computer.
 *
 * @return -1 and errno on error, 0 if it's connected 
 */
int
tel_is_ready (void)
{
  char rbuf[6];
  if (tel_write_read ("#:Gc#", 5, rbuf, 5) < 0)
    return -1;
  rbuf[5] = 0;
  if (strncmp (rbuf, "(12)#", 5) && strncmp (rbuf, "(24)#", 5))
    {
      errno = EIO;
      return -1;
    }
  return tel_init ();
}

/*! 
 * Park telescope to neutral location.
 *
 * @return -1 and errno on error, 0 otherwise
 */
int
tel_park (void)
{
  double lst;
  if (tel_read_siderealtime (&lst) < 0)
    return -1;
  return tel_move_to (lst, 40);
}
