/* 
 * Class for PIXY lightening detector.
 * 
 * Copyright (C) 2007 Stanislav Vitek
 * Copyright (C) 2008 Petr Kubanek <petr@kubanek.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "sensord.h"
#include "../utils/rts2connserial.h"

class Rts2SensorPixy;

/**
 * Sensor connection.
 *
 * As pixy sensor put character on port every second (=it is not polled), we
 * have to prepare special connection, which will handle processLine its own way..
 */
class Rts2ConnPixy:public Rts2ConnSerial
{
	private:
		/**
		 * Last value from the connection.
		 */
		char lastVal;
	protected:
		virtual int receive (fd_set *set);
	public:
		Rts2ConnPixy (const char *in_devName, Rts2SensorPixy * in_master,
			bSpeedT in_baudSpeed = BS9600, cSizeT in_cSize = C8,
			parityT in_parity = NONE, int in_vTime = 40);
};

/**
 * Class for lighting detector.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Rts2SensorPixy:public Rts2DevSensor
{
	private:
		char *device_port;
		Rts2ConnPixy *pixyConn;

		Rts2ValueInteger *lightening;

		virtual int processOption (int in_opt);
		virtual int init ();
		virtual int info ();
	public:
		Rts2SensorPixy (int argc, char **argv);
		virtual ~Rts2SensorPixy (void);

		/**
		 * Called when PIXY receives some data..
		 */
		void pixyReceived (char lastVal);
};


int
Rts2ConnPixy::receive (fd_set *set)
{
	if (sock < 0 || !FD_ISSET (sock, set))
	 	return 0;
	// get one byte..
	if (readPort (lastVal) != 1)
		return 0;

	((Rts2SensorPixy *)getMaster ())->pixyReceived (lastVal);
		
	return 1;
}


Rts2ConnPixy::Rts2ConnPixy (const char *in_devName, Rts2SensorPixy * in_master,
			bSpeedT in_baudSpeed, cSizeT in_cSize,
			parityT in_parity, int in_vTime)
:Rts2ConnSerial (in_devName, in_master, in_baudSpeed, in_cSize, in_parity, in_vTime)
{
}


int
Rts2SensorPixy::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'f':
			device_port = optarg;
			break;
		default:
			return Rts2DevSensor::processOption (in_opt);
	}
	return 0;
}

int
Rts2SensorPixy::init ()
{
	int ret;
	ret = Rts2DevSensor::init ();
	if (ret)
		return ret;
	
	pixyConn = new Rts2ConnPixy (device_port, this, BS19200, C8, NONE, 20);
	addConnection (pixyConn);
	ret = pixyConn->init ();
	if (ret)
		return ret;
	return 0;
}


int
Rts2SensorPixy::info ()
{
	// do not set infotime
	return 0;
}


Rts2SensorPixy::Rts2SensorPixy (int argc, char **argv)
:Rts2DevSensor (argc, argv)
{
	pixyConn = NULL;

	createValue (lightening, "lightening_index", "0-9 value describing number of detected strokes", true);

	addOption ('f', NULL, 1, "device port (default to /dev/ttyS0");
}


Rts2SensorPixy::~Rts2SensorPixy (void)
{
}


void
Rts2SensorPixy::pixyReceived (char lastVal)
{
	lightening->setValueInteger (lastVal - '0');
	sendValueAll (lightening);
	updateInfoTime ();
}


int
main (int argc, char **argv)
{
	Rts2SensorPixy device = Rts2SensorPixy (argc, argv);
	return device.run ();
}
