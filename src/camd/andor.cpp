/*
 * Andor driver (optimised/specialised for iXon model
 * Copyright (C) 2005-2007 Petr Kubanek <petr@kubanek.net>
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camd.h"

#ifdef __GNUC__
#  if(__GNUC__ > 3 || __GNUC__ ==3)
# define _GNUC3_
#  endif
#endif

#ifdef _GNUC3_
#  include <iostream>
#  include <fstream>
using namespace std;
#else
#  include <iostream.h>
#  include <fstream.h>
#endif

#include "atmcdLXd.h"
//That's root for andor2.77
#define ANDOR_ROOT "/usr/local/etc/andor"

#define IXON_DEFAULT_GAIN 255
#define IXON_MAX_GAIN 255

#define ANDOR_SHUTTER_AUTO 0
#define ANDOR_SHUTTER_OPEN 1
#define ANDOR_SHUTTER_CLOSED 2

/**
 * Andor camera, as seen by the outside world.
 *
 * This was originally written for an iXon, and doesn't handle anything
 * an iXon can't do.  If used with a different Andor camera, it should
 * respond reasonably well to the absence of iXon features
 *
 */
class Rts2DevCameraAndor:public Rts2DevCamera
{
	private:
		char *andorRoot;
		bool printSpeedInfo;
		// number of AD channels
		int chanNum;

		AndorCapabilities cap;
		int andor_shutter_state;

		int disable_ft;
		int shutter_with_ft;

		Rts2ValueInteger *gain;

		Rts2ValueBool *useFT;
		Rts2ValueInteger *VSAmp;
		Rts2ValueBool *FTShutter;

		// informational values
		Rts2ValueInteger *ADChannel;
		Rts2ValueBool *EMOn;
		Rts2ValueInteger *HSpeed;
		Rts2ValueInteger *VSpeed;
		Rts2ValueFloat *HSpeedHZ;
		Rts2ValueFloat *VSpeedHZ;
		Rts2ValueInteger *bitDepth;
		Rts2ValueInteger *acqusitionMode;

		int defaultGain;

		void getTemp ();
		int setGain (int in_gain);
		int setADChannel (int in_adchan);
		int setVSAmplitude (int in_vsamp);
		int setHSSpeed (int in_amp, int in_hsspeed);
		int setVSSpeed (int in_vsspeed);
		int setFTShutter (bool force);
		int setAcquisitionMode (int mode);

		int printInfo ();
		void printCapabilities ();
		int printNumberADCs ();
		int printHSSpeeds (int camera_type, int ad_channel, int amplifier);
		int printVSSpeeds ();

		void initAndorValues ();
		void closeShutter ();

	protected:
		virtual int processOption (int in_opt);
		virtual void help ();
		virtual void cancelPriorityOperations ();

		virtual void initDataTypes ();

		virtual int startExposure ();
		virtual int stopExposure ();
		virtual long isExposing ();

		virtual int setValue (Rts2Value * old_value, Rts2Value * new_value);

		virtual int readoutOneLine ();

	public:
		Rts2DevCameraAndor (int argc, char **argv);
		virtual ~Rts2DevCameraAndor (void);

		virtual int initChips ();
		virtual int init ();

		virtual bool supportFrameTransfer ();

		// callback functions for Camera alone
		virtual int ready ();
		virtual int info ();
		virtual int scriptEnds ();
		virtual int camChipInfo (int chip);
		virtual int camCoolMax ();
		virtual int camCoolHold ();
		virtual int camCoolTemp (float new_temp);
		virtual int camCoolShutdown ();
};

int
Rts2DevCameraAndor::stopExposure ()
{
	AbortAcquisition ();
	FreeInternalMemory ();
	return Rts2DevCamera::stopExposure ();
}


long
Rts2DevCameraAndor::isExposing ()
{
	int status;
	int ret;
	// if we are in acqusition mode 5
	if (acqusitionMode->getValueInteger () == 5)
	{
		long first, last;
		if (GetNumberNewImages (&first, &last) == DRV_NO_NEW_DATA)
		{
			return 100;
		}
		// there is new image, process it
		std::cout << "Images " << first << " " << last << std::endl;
		for (long i = first; i <= last; i++)
		{
			// signal that we have data
			maskStateChip (0, CAM_MASK_EXPOSE | CAM_MASK_READING, CAM_NOEXPOSURE | CAM_READING, BOP_TEL_MOVE, 0, "chip extended readout started");
			long firstValid, lastValid;
			// get the data
			GetSizeOfCircularBuffer (&firstValid);
			std::cout << "buffer size " << firstValid << std::endl;
			switch (getDataType ())
			{
				case RTS2_DATA_LONG:
					if (GetImages (i, i, (long *) dataBuffer, chipUsedSize (), &firstValid, &lastValid) != DRV_SUCCESS)
					{
						logStream (MESSAGE_ERROR) << "Cannot get long data" << sendLog;
						return -1;
					}
					break;
				default:
					if (GetImages16 (i, i, (unsigned short *) dataBuffer, chipUsedSize (), &firstValid, &lastValid) != DRV_SUCCESS)
					{
						logStream (MESSAGE_ERROR) << "Cannot get int data" << sendLog;
						return -1;
					}
			}
			// now send the data
			ret = sendImage (dataBuffer, dataBufferSize);
			if (ret)
				return ret;
			maskStateChip (0, CAM_MASK_EXPOSE | CAM_MASK_READING, CAM_EXPOSING | CAM_NOTREADING, BOP_TEL_MOVE, 0, "chip extended readout started");
			if (quedExpNumber->getValueInteger () == 0)
			{
				// stop exposure if we do not have any qued values
				AbortAcquisition ();
				FreeInternalMemory ();
				logStream (MESSAGE_INFO) << "Aborting acqusition" << sendLog;
				return -1;
			}
			quedExpNumber->dec ();
		}
		return 100;
	}
	if ((ret = Rts2DevCamera::isExposing ()) != 0)
		return ret;
	if (GetStatus (&status) != DRV_SUCCESS)
		return -1;
	if (status == DRV_ACQUIRING)
		return 100;
	return 0;
}


// For each exposure, the first time this function is called, it reads out
// the entire image from the camera (into dest).  Subsequent calls return
// lines from dest.
int
Rts2DevCameraAndor::readoutOneLine ()
{
	int ret;
	switch (getDataType ())
	{
		case RTS2_DATA_FLOAT:
			ret = GetAcquiredFloatData ((float *)dataBuffer, chipUsedSize ());
			break;
		case RTS2_DATA_LONG:
			ret = GetAcquiredData ((long *)dataBuffer, chipUsedSize ());
			break;
			// case RTS2_DATA_SHORT:
		default:
			ret = GetAcquiredData16 ((short unsigned *)dataBuffer, chipUsedSize ());
			break;
	}
	if (ret != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) << "andor GetAcquiredXXXX "
			<< getDataType () << " return " << ret << sendLog;
		return -1;
	}
	ret = sendReadoutData (dataBuffer, dataBufferSize);
	if (ret < 0)
		return -1;
	if (getWriteBinaryDataSize () == 0)
		return -2;
	return 0;
}


bool
Rts2DevCameraAndor::supportFrameTransfer ()
{
	return useFT->getValueBool () && (cap.ulAcqModes & AC_ACQMODE_FRAMETRANSFER);
}


void
Rts2DevCameraAndor::closeShutter ()
{
	SetShutter (1, ANDOR_SHUTTER_CLOSED, 50, 50);
	andor_shutter_state = ANDOR_SHUTTER_CLOSED;
}


Rts2DevCameraAndor::Rts2DevCameraAndor (int in_argc, char **in_argv):
Rts2DevCamera (in_argc, in_argv)
{
	createTempCCD ();
	createTempRegulation ();
	createTempSet ();

	createExpType ();

	andorRoot = ANDOR_ROOT;

	gain = NULL;

	createValue (ADChannel, "ADCHANEL",
		"Used andor AD Channel, on ixon 0 for 14 bit, 1 for 16 bit",
		true, 0, CAM_WORKING, true);
	ADChannel->setValueInteger (0);
	// create VSAmp only if we have amplitude set capability
	VSAmp = NULL;
	createValue (VSpeed, "VSPEED", "Vertical shift speed", true, 0,
		CAM_WORKING, true);
	VSpeed->setValueInteger (1);
	createValue (EMOn, "EMON", "If EM is enabled", true, 0,
		CAM_WORKING, true);
	EMOn->setValueBool (true);
	createValue (HSpeed, "HSPEED", "Horizontal shift speed", true, 0,
		CAM_WORKING, true);
	HSpeed->setValueInteger (1);
	createValue (FTShutter, "FTSHUT", "Use shutter, even with FT", true, 0,
		CAM_WORKING, true);
	FTShutter->setValueBool (false);

	createValue (useFT, "USEFT", "Use FT", true, 0, CAM_WORKING, true);
	useFT->setValueBool (true);

	createValue (acqusitionMode, "ACQMODE", "acqusition mode", true, 0, CAM_WORKING, true);

	defaultGain = IXON_DEFAULT_GAIN;

	printSpeedInfo = false;

	addOption ('m', "mode", 1, "Which mode to use");
	addOption ('r', "root", 1, "directory with Andor detector.ini file");
	addOption ('g', "gain", 1, "set camera gain level (0-255)");
	addOption ('N', "noft", 0, "do not use frame transfer mode");
	addOption ('I', "speed_info", 0,
		"print speed info - information about speed available");
	addOption ('S', "ft-uses-shutter", 0, "force use of shutter with FT");
}


Rts2DevCameraAndor::~Rts2DevCameraAndor (void)
{
	ShutDown ();
}


void
Rts2DevCameraAndor::help ()
{
	std::cout << "Driver for Andor CCDs (iXon & others)" << std::endl;
	std::
		cout <<
		"Optimal values for vertical speed on iXon are: -H 1 -v 1 -C 1, those are default"
		<< std::endl;
	Rts2DevCamera::help ();
}


int
Rts2DevCameraAndor::setGain (int in_gain)
{
	int ret;
	if ((ret = SetEMCCDGain (in_gain)) != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) << "andor setGain error " << ret << sendLog;
		return -1;
	}
	gain->setValueInteger (in_gain);
	return 0;
}


int
Rts2DevCameraAndor::setADChannel (int in_adchan)
{
	int ret;
	if ((ret = SetADChannel (in_adchan)) != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) << "andor setADChannel error " << ret <<
			sendLog;
		return -1;
	}
	ADChannel->setValueInteger (in_adchan);
	return 0;
}


int
Rts2DevCameraAndor::setVSAmplitude (int in_vsamp)
{
	int ret;
	if ((ret = SetVSAmplitude (in_vsamp)) != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) << "andor setVSAmplitude error " << ret <<
			sendLog;
		return -1;
	}
	VSAmp->setValueInteger (in_vsamp);
	return 0;
}


int
Rts2DevCameraAndor::setHSSpeed (int in_amp, int in_hsspeed)
{
	int ret;
	if ((ret = SetHSSpeed (in_amp, in_hsspeed)) != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) << "andor setHSSpeed amplifier " << in_amp << " speed " << in_hsspeed << " error " << ret <<
			sendLog;
		return -1;
	}
	EMOn->setValueBool (in_amp == 0 ? true : false);
	HSpeed->setValueInteger (in_hsspeed);
	return 0;

}


int
Rts2DevCameraAndor::setVSSpeed (int in_vsspeed)
{
	int ret;
	if ((ret = SetVSSpeed (in_vsspeed)) != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) << "andor setVSSpeed error " << ret <<
			sendLog;
		return -1;
	}
	VSpeed->setValueInteger (in_vsspeed);
	return 0;
}


int
Rts2DevCameraAndor::setFTShutter (bool force)
{
	FTShutter->setValueBool (force);
	return 0;
}


int
Rts2DevCameraAndor::setAcquisitionMode (int mode)
{
	if (SetAcquisitionMode (mode) != DRV_SUCCESS)
		return -1;
	acqusitionMode->setValueInteger (mode);
	return 0;
}


void
Rts2DevCameraAndor::cancelPriorityOperations ()
{
	if (!isnan (defaultGain) && gain)
		setGain (defaultGain);
	Rts2DevCamera::cancelPriorityOperations ();
}


void
Rts2DevCameraAndor::initDataTypes ()
{
	Rts2DevCamera::initDataTypes ();
	addDataType (RTS2_DATA_LONG);
	addDataType (RTS2_DATA_FLOAT);
}


int
Rts2DevCameraAndor::startExposure ()
{
	int ret;

	ret = SetImage (binningHorizontal (), binningVertical (), chipTopX () + 1,
		chipTopX () + chipUsedReadout->getHeightInt (),
		chipTopY () + 1,
		chipTopY () + chipUsedReadout->getWidthInt ());
	if (ret != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) << "andor SetImage return " << ret << sendLog;
		return -1;
	}

	float acq_exp, acq_acc, acq_kinetic;
	if (isnan (getSubExposure ()))
	{
		if (supportFrameTransfer ())
		{
			if (setAcquisitionMode (5))
			{
				logStream (MESSAGE_ERROR) << "Cannot set AQ run-till-abort mode" << sendLog;
				return -1;
			}
			if (SetKineticCycleTime (0) != DRV_SUCCESS)
			{
				logStream (MESSAGE_ERROR) << "Cannot set kinetic timing to 0" << sendLog;
				return -1;
			}
			if (GetAcquisitionTimings (&acq_exp, &acq_acc, &acq_kinetic) != DRV_SUCCESS)
				return -1;
			std::cout << "timings " << acq_exp << " " << acq_acc << " " << acq_kinetic << std::endl;
		}
		else
		{
			// single scan
			if (setAcquisitionMode (AC_ACQMODE_SINGLE))
			{
				logStream (MESSAGE_ERROR) << "Cannot set AQ mode" << sendLog;
				return -1;
			}
		}
		if (SetExposureTime (getExposure ()) != DRV_SUCCESS)
		{
			logStream (MESSAGE_ERROR) << "Cannot set exposure time" << sendLog;
			return -1;
		}
	}
	else
	{
		nAcc = (int) (getExposure () / getSubExposure ());
		if (nAcc == 0)
		{
			nAcc = 1;
			setSubExposure (getExposure ());
		}

		// Acquisition mode 2 is "accumulate"
		if (setAcquisitionMode (2))
			return -1;
		if (SetExposureTime (getSubExposure ()) != DRV_SUCCESS)
			return -1;
		if (SetNumberAccumulations (nAcc) != DRV_SUCCESS)
			return -1;
		if (GetAcquisitionTimings (&acq_exp, &acq_acc, &acq_kinetic) !=
			DRV_SUCCESS)
			return -1;
		setExposure (nAcc * acq_exp);
		setSubExposure (acq_exp);
	}

	int new_state = (getExpType () == 0) ? ANDOR_SHUTTER_AUTO : ANDOR_SHUTTER_CLOSED;

	if ((getExpType () == 0) && (supportFrameTransfer ()) && (!FTShutter->getValueBool ()))
		new_state = ANDOR_SHUTTER_OPEN;

	if (new_state != andor_shutter_state)
	{
		logStream (MESSAGE_DEBUG) << "SetShutter " << new_state << sendLog;
		ret = SetShutter (1, new_state, 50, 50);
		if (ret != DRV_SUCCESS)
		{
			logStream (MESSAGE_ERROR) << "Cannot set shutter state to " <<
				new_state << " error " << ret << sendLog;
			return -1;
		}
	}
	andor_shutter_state = new_state;

	getTemp ();

	if ((ret = StartAcquisition ()) != DRV_SUCCESS)
		return -1;
	return 0;
}


// scriptEnds
// Ensure that we definitely leave the shutter closed.
int
Rts2DevCameraAndor::scriptEnds ()
{
	//	closeShutter ();
	return Rts2DevCamera::scriptEnds ();
}


int
Rts2DevCameraAndor::setValue (Rts2Value * old_value, Rts2Value * new_value)
{
	if (old_value == gain)
		return setGain (new_value->getValueInteger ());
	if (old_value == ADChannel)
		return setADChannel (new_value->getValueInteger ()) == 0 ? 0 : -2;
	if (old_value == VSAmp)
		return setVSAmplitude (new_value->getValueInteger ()) == 0 ? 0 : -2;
	if (old_value == EMOn)
		return setHSSpeed (((Rts2ValueBool *) new_value)->getValueBool ()? 0 : 1,
		HSpeed->getValueInteger ()) == 0 ? 0 : -2;
	if (old_value == HSpeed)
		return setHSSpeed (EMOn->getValueBool ()? 0 : 1,
		new_value->getValueInteger ()) == 0 ? 0 : -2;
	if (old_value == VSpeed)
		return setVSSpeed (new_value->getValueInteger ()) == 0 ? 0 : -2;
	if (old_value == FTShutter)
		return setFTShutter (((Rts2ValueBool *) new_value)->getValueBool ()) ==
			0 ? 0 : -2;
	if (old_value == useFT)
	{
		int status;
		if (!(cap.ulAcqModes & AC_ACQMODE_FRAMETRANSFER))
			return -2;
		status = SetFrameTransferMode (((Rts2ValueBool *) new_value)->getValueBool () ? 1 : 0);
		if (status != DRV_SUCCESS)
			return -2;
		return 0;
	}

	return Rts2DevCamera::setValue (old_value, new_value);
}


int
Rts2DevCameraAndor::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'g':
			defaultGain = atoi (optarg);
			if (defaultGain > IXON_MAX_GAIN || defaultGain < 0)
			{
				printf ("gain must be in 0-255 range\n");
				exit (EXIT_FAILURE);
			}
			break;
		case 'r':
			andorRoot = optarg;
			break;
		case 'I':
			printSpeedInfo = true;
			break;
		case 'N':
			useFT->setValueBool (false);
			break;
		case 'S':
			shutter_with_ft = true;
		default:
			return Rts2DevCamera::processOption (in_opt);
	}
	return 0;
}


/*******************************************************************
 * printInfo (multiple functions)
 *
 * Do a full probe of what the attached camera can do, and print it out.
 * Note that an amount of stuff is duplicated between here and CameraAndorChip
 * so if/when that gets merged, some info may aready be available.
 *
 */

void
Rts2DevCameraAndor::printCapabilities ()
{
	printf ("Acquisition modes: ");
	if (cap.ulAcqModes == 0)
		printf ("<none>");
	if (cap.ulAcqModes & AC_ACQMODE_SINGLE)
		printf (" SINGLE");
	if (cap.ulAcqModes & AC_ACQMODE_VIDEO)
		printf (" VIDEO");
	if (cap.ulAcqModes & AC_ACQMODE_ACCUMULATE)
		printf (" ACCUMULATE");
	if (cap.ulAcqModes & AC_ACQMODE_KINETIC)
		printf (" KINETIC");
	if (cap.ulAcqModes & AC_ACQMODE_FRAMETRANSFER)
		printf (" FRAMETRANSFER");
	if (cap.ulAcqModes & AC_ACQMODE_FASTKINETICS)
		printf (" FASTKINETICS");

	printf ("\nRead modes: ");
	if (cap.ulReadModes & AC_READMODE_FULLIMAGE)
		printf (" FULLIMAGE");
	if (cap.ulReadModes & AC_READMODE_SUBIMAGE)
		printf (" SUBIMAGE");
	if (cap.ulReadModes & AC_READMODE_SINGLETRACK)
		printf (" SINGLETRACK");
	if (cap.ulReadModes & AC_READMODE_FVB)
		printf (" FVB");
	if (cap.ulReadModes & AC_READMODE_MULTITRACK)
		printf (" MULTITRACK");
	if (cap.ulReadModes & AC_READMODE_RANDOMTRACK)
		printf (" RANDOMTRACK");

	printf ("\nTrigger modes: ");
	if (cap.ulTriggerModes & AC_TRIGGERMODE_INTERNAL)
		printf (" INTERNAL");
	if (cap.ulTriggerModes & AC_TRIGGERMODE_EXTERNAL)
		printf (" EXTERNAL");

	printf ("\nPixel modes: ");
	if (cap.ulPixelMode & AC_PIXELMODE_8BIT)
		printf (" 8BIT");
	if (cap.ulPixelMode & AC_PIXELMODE_14BIT)
		printf (" 14BIT");
	if (cap.ulPixelMode & AC_PIXELMODE_16BIT)
		printf (" 16BIT");
	if (cap.ulPixelMode & AC_PIXELMODE_32BIT)
		printf (" 32BIT");
	if (cap.ulPixelMode & AC_PIXELMODE_32BIT)
		printf (" 32BIT");
	if (cap.ulPixelMode & AC_PIXELMODE_MONO)
		printf (" MONO");
	if (cap.ulPixelMode & AC_PIXELMODE_RGB)
		printf (" RGB");
	if (cap.ulPixelMode & AC_PIXELMODE_CMY)
		printf (" CMY");

	printf ("\nSettable variables: ");
	if (cap.ulSetFunctions & AC_SETFUNCTION_VREADOUT)
		printf (" VREADOUT");
	if (cap.ulSetFunctions & AC_SETFUNCTION_HREADOUT)
		printf (" HREADOUT");
	if (cap.ulSetFunctions & AC_SETFUNCTION_TEMPERATURE)
		printf (" TEMPERATURE");
	if (cap.ulSetFunctions & AC_SETFUNCTION_GAIN)
		printf (" GAIN");
	if (cap.ulSetFunctions & AC_SETFUNCTION_EMCCDGAIN)
		printf (" EMCCDGAIN");

	printf ("\n");
}


/****************************************************************
 * printNumberADCs
 *
 * Prints out number and bit-depth of available AD channels, and returns
 * the number of AD channels found, 0 on error.
 */

int
Rts2DevCameraAndor::printNumberADCs ()
{
	int ret, n_ad;
	if ((ret = GetNumberADChannels (&n_ad)) != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) <<
			"andor cannot get number of AD channels" << sendLog;
		return -1;
	}
	printf ("AD Channels: %d (", n_ad);
	for (int ad = 0; ad < n_ad; ad++)
	{
		int depth;
		if ((ret = GetBitDepth (ad, &depth)) != DRV_SUCCESS)
		{
			logStream (MESSAGE_ERROR) <<
				"andor cannot get depth for ad " << ad << sendLog;
			return -1;
		}
		if (n_ad > 1)
			printf ("%d=%d-bit", ad, depth);
		else
			printf ("%d-bit", depth);
		if (ad == (n_ad - 1))
			printf (")\n");
		else
			printf (", ");
	}
	return n_ad;
}


int
Rts2DevCameraAndor::printHSSpeeds (int camera_type, int ad, int amp)
{
	int ret, nhs;
	if ((ret = GetNumberHSSpeeds (ad, amp, &nhs)) != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) <<
			"andor cannot get number of horizontal speeds " << sendLog;
		return -1;
	}

	printf ("Horizontal speeds: %d (", nhs);

	for (int s = 0; s < nhs; s++)
	{
		float val;
		if ((ret = GetHSSpeed (ad, amp, s, &val)) != DRV_SUCCESS)
		{
			logStream (MESSAGE_ERROR) <<
				"andor cannot get horizontal speed " << s <<
				" ad " << ad << " amp " << amp << sendLog;
			return -1;
		}
		printf ("%.2f", val);
		if (s == (nhs - 1))
			switch (camera_type)
			{
				case AC_CAMERATYPE_IXON:
					printf (" MHz)\n");
					break;
				default:
					printf (" usec/pix)\n");
					break;
			}
			else
				printf (", ");
	}
	return 0;
}


int
Rts2DevCameraAndor::printVSSpeeds ()
{
	int ret, vspeeds;
	if ((ret = GetNumberVSSpeeds (&vspeeds)) != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) <<
			"andor init cannot get vertical speeds" << sendLog;
		return -1;
	}
	printf ("Vertical Speeds: %d (", vspeeds);
	for (int s = 0; s < vspeeds; s++)
	{
		float val;
		GetVSSpeed (s, &val);
		printf ("%.2f", val);
		if (s == (vspeeds - 1))
			printf (" usec/pix)\n");
		else
			printf (", ");
	}
	return 0;
}


int
Rts2DevCameraAndor::printInfo ()
{
	int ret;
	int n_ad, n_amp;
	char name[128];

	printf ("Camera type: ");
	switch (cap.ulCameraType)
	{
		case AC_CAMERATYPE_PDA:
			printf ("PDA");
			break;
		case AC_CAMERATYPE_IXON:
			printf ("IXON");
			break;
		case AC_CAMERATYPE_ICCD:
			printf ("ICCD");
			break;
		case AC_CAMERATYPE_EMCCD:
			printf ("EMCCD");
			break;
		case AC_CAMERATYPE_CCD:
			printf ("CCD");
			break;
		case AC_CAMERATYPE_ISTAR:
			printf ("ISTAR");
			break;
		case AC_CAMERATYPE_VIDEO:
			printf ("VIDEO");
			break;
		default:
			printf ("<unknown> (code is %lu)", cap.ulCameraType);
			break;
	}

	GetHeadModel (name);
	printf (" Model: %s\n", name);

	printCapabilities ();

	if ((n_ad = printNumberADCs ()) < 1)
		return -1;

	GetNumberAmp (&n_amp);
	printf ("Output amplifiers: %d\n", n_amp);

	for (int ad = 0; ad < n_ad; ad++)
		for (int amp = 0; amp < n_amp; amp++)
			if ((ret = printHSSpeeds (cap.ulCameraType, ad, amp)) != 0)
				return ret;

	if ((ret = printVSSpeeds ()) != 0)
		return ret;

	return 0;
}


int
Rts2DevCameraAndor::initChips ()
{
	int ret;
	float x_um, y_um;
	int x_pix, y_pix;

	SetShutter (1, ANDOR_SHUTTER_CLOSED, 50, 50);
	andor_shutter_state = ANDOR_SHUTTER_CLOSED;

	if ((ret = GetPixelSize (&x_um, &y_um)) != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) <<
			"andor chip cannot get pixel size" << ret << sendLog;
		return -1;
	}

	//GetPixelSize returns floats, pixel[XY] are doubles
	pixelX = x_um;
	pixelY = y_um;

	if ((ret = GetDetector (&x_pix, &y_pix)) != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) <<
			"andor chip cannot get detector size" << ret << sendLog;
		return -1;
	}
	setSize (x_pix, y_pix, 0, 0);

	if ((ret = GetCapabilities (&cap)) != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) <<
			"andor chip failed to retrieve camera capabilities" << ret << sendLog;
		return -1;
	}

	// use frame transfer mode
	if (supportFrameTransfer ()
		&& ((ret = SetFrameTransferMode (1)) != DRV_SUCCESS))
	{
		logStream (MESSAGE_ERROR) <<
			"andor init attempt to set frame transfer failed " << ret << sendLog;
		return -1;
	}
	return 0;
}


int
Rts2DevCameraAndor::init ()
{
	unsigned long err;
	int ret;

	if ((ret = Rts2DevCamera::init ()) != 0)
		return ret;

	err = Initialize (andorRoot);
	if (err != DRV_SUCCESS)
	{
		cerr << "Andor library init failed (code " << err << "). exiting" <<
			endl;
		return -1;
	}

	sleep (2);					 //sleep to allow initialization to complete

	SetExposureTime (5.0);

	ret = GetCapabilities (&cap);
	if (ret != DRV_SUCCESS)
	{
		cerr << "Cannot call GetCapabilities " << ret << endl;
		return -1;
	}

	initAndorValues ();

	//Set Read Mode to --Image--
	ret = SetReadMode (4);
	if (ret != DRV_SUCCESS)
	{
		cerr << "Cannot set read mode (" << ret << "), exiting" << endl;
		return -1;
	}

	if (printSpeedInfo)
		printInfo ();

	sprintf (ccdType, "ANDOR");

	return initChips ();
}


void
Rts2DevCameraAndor::initAndorValues ()
{
	if (cap.ulCameraType == AC_CAMERATYPE_IXON)
	{
		createValue (VSAmp, "SAMPLI", "Used andor shift amplitude", true, 0, CAM_WORKING, true);
		VSAmp->setValueInteger (0);
	}
	if (cap.ulSetFunctions & AC_SETFUNCTION_EMCCDGAIN)
	{
		createValue (gain, "GAIN", "CCD gain", true, 0,
			CAM_WORKING, true);
		setGain (defaultGain);
	}
}


int
Rts2DevCameraAndor::ready ()
{
	return 0;
}


void
Rts2DevCameraAndor::getTemp ()
{
	int c_status;
	float tmpTemp;
	c_status = GetTemperatureF (&tmpTemp);
	if (!
		(c_status == DRV_ACQUIRING || c_status == DRV_NOT_INITIALIZED
		|| c_status == DRV_ERROR_ACK))
	{
		tempCCD->setValueDouble (tmpTemp);
		tempRegulation->setValueInteger (c_status != DRV_TEMPERATURE_OFF);
	}
	else
	{
		logStream (MESSAGE_DEBUG) << "andor info status " << c_status <<
			sendLog;
	}
}


int
Rts2DevCameraAndor::info ()
{
	if (isIdle ())
	{
		getTemp ();
	}
	return Rts2DevCamera::info ();
}


int
Rts2DevCameraAndor::camChipInfo (int chip)
{
	return 0;
}


int
Rts2DevCameraAndor::camCoolMax ()
{
	return camCoolHold ();
}


int
Rts2DevCameraAndor::camCoolHold ()
{
	if (isnan (nightCoolTemp))
		return camCoolTemp (-5);
	else
		return camCoolTemp (nightCoolTemp);
}


int
Rts2DevCameraAndor::camCoolTemp (float new_temp)
{
	int status;
	status = CoolerON ();
	if (status != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) << "Cannot switch cooler to on, status: " << status << sendLog;
		return -1;
	}
	status = SetTemperature ((int) new_temp);
	if (status != DRV_SUCCESS)
	{
		logStream (MESSAGE_ERROR) << "Cannot set cooling tempereture, status: " << status << sendLog;
		return -1;
	}
	tempSet->setValueDouble (new_temp);
	return 0;
}


int
Rts2DevCameraAndor::camCoolShutdown ()
{
	CoolerOFF ();
	SetTemperature (20);
	tempSet->setValueDouble (+50);
	closeShutter ();
	return 0;
}


int
main (int argc, char **argv)
{
	Rts2DevCameraAndor device = Rts2DevCameraAndor (argc, argv);
	return device.run ();
}
