/* 
 * Driver for Arduino laser source.
 * Copyright (C) 2010 Petr Kubanek <petr@kubanek.net>
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

#include "../utils/connserial.h"

namespace rts2sensord
{

/**
 * Arduino used as simple I/O board. Arduino shuld run rts2.pde code.
 *
 * @author Petr Kubanek <kubanek@fzu.cz>
 */
class Arduino:public Sensor
{
	public:
		Arduino (int argc, char **argv);

		virtual int commandAuthorized (Rts2Conn *conn);
	protected:
		virtual int processOption (int opt);
		virtual int init ();
		virtual int info ();

	private:
		char *device_file;
		rts2core::ConnSerial *arduinoConn;

		Rts2ValueBool *decHome;
		Rts2ValueBool *raHome;
		Rts2ValueBool *raLimit;

		Rts2ValueFloat *a1_x;
		Rts2ValueFloat *a1_y;
		Rts2ValueFloat *a1_z;

		Rts2ValueFloat *a2_x;
		Rts2ValueFloat *a2_y;
		Rts2ValueFloat *a2_z;
};

}

using namespace rts2sensord;

Arduino::Arduino (int argc, char **argv): Sensor (argc, argv)
{
	device_file = NULL;
	arduinoConn = NULL;

	createValue (decHome, "DEC_HOME", "DEC axis home sensor", false, RTS2_DT_ONOFF);
	createValue (raHome, "RA_HOME", "RA axis home sensor", false, RTS2_DT_ONOFF);
	createValue (raLimit, "RA_LIMIT", "RA limit switch", false, RTS2_DT_ONOFF);

	createValue (a1_x, "RA_X", "RA accelometer X", false);
	createValue (a1_y, "RA_Y", "RA accelometer Y", false);
	createValue (a1_z, "RA_Z", "RA accelometer Z", false);

	createValue (a2_x, "DEC_X", "DEC accelometer X", false);
	createValue (a2_y, "DEC_Y", "DEC accelometer Y", false);
	createValue (a2_z, "DEC_Z", "DEC accelometer Z", false);

	addOption ('f', NULL, 1, "serial port with the module (ussually /dev/ttyUSB for Arduino USB serial connection");

	setIdleInfoInterval (1);
}

int Arduino::commandAuthorized (Rts2Conn * conn)
{
	if (conn->isCommand ("reset"))
	{
		setStopState (false, "reseted");
		return 0;
	}
	return Sensor::commandAuthorized (conn);
}

int Arduino::processOption (int opt)
{
	switch (opt)
	{
		case 'f':
			device_file = optarg;
			break;
		default:
			return Sensor::processOption (opt);
	}
	return 0;
}

int Arduino::init ()
{
	int ret;
	ret = Sensor::init ();
	if (ret)
		return ret;

	if (device_file == NULL)
	{
		logStream (MESSAGE_ERROR) << "you must specify device file (TTY port)" << sendLog;
		return -1;
	}

	arduinoConn = new rts2core::ConnSerial (device_file, this, rts2core::BS9600, rts2core::C8, rts2core::NONE, 10);
	ret = arduinoConn->init ();
	if (ret)
		return ret;
	
	arduinoConn->flushPortIO ();
	arduinoConn->setDebug (false);

	return 0;
}

int Arduino::info ()
{
	char buf[100];
	int ret = arduinoConn->writeRead ("?", 1, buf, 99, '\n');
	if (ret < 0)
		return -1;

	int i;
	int a0,a1,a2,a3,a4,a5;

	if (sscanf (buf, "%d %d %d %d %d %d %d", &i, &a0, &a1, &a2, &a3, &a4, &a5) != 7)
	{
		buf[ret] = '\0';
		logStream (MESSAGE_ERROR) << "invalid reply from arudiono: " << buf << sendLog;
		return -1;
	}

	raLimit->setValueBool (i & 0x01);
	raHome->setValueBool (i & 0x02);
	decHome->setValueBool (i & 0x04);

	a1_x->setValueFloat (a0 / 450.0);
	a1_y->setValueFloat (a1 / 450.0);
	a1_z->setValueFloat (a2 / 450.0);

	a2_x->setValueFloat (a3 / 450.0);
	a2_y->setValueFloat (a4 / 450.0);
	a2_z->setValueFloat (a5 / 450.0);

	if (raLimit->getValueBool ())
		setStopState (true, "RA axis beyond limits");

	return Sensor::info ();
}

int main (int argc, char **argv)
{
	Arduino device (argc, argv);
	return device.run ();
}
