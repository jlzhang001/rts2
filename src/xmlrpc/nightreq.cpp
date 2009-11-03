/*
 * Classes for generating pages with observations by nights.
 * Copyright (C) 2009 Petr Kubanek <petr@kubanek.net>
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

#include "nightreq.h"
#include "nightdur.h"

#ifdef HAVE_PGSQL
#include "../utilsdb/observationset.h"
#if defined(HAVE_LIBJPEG) && HAVE_LIBJPEG == 1
#include "altaz.h"
#endif // HAVE_LIBJPEG
#include "../utils/rts2config.h"

using namespace XmlRpc;
using namespace rts2xmlrpc;

void Night::authorizedExecute (std::string path, XmlRpc::HttpParams *params, const char* &response_type, char* &response, int &response_length)
{
	response_type = "text/html";

	// get path and possibly date range
	std::vector <std::string> vals = SplitStr (path.substr (1), std::string ("/"));
	int year = -1;
	int month = -1;
	int day = -1;

	switch (vals.size ())
	{
		case 4:
			// assumes that all previous are OK, get just target
			printObs (atoi (vals[3].c_str ()), response_type, response, response_length);
			break;			
		case 3:
			day = atoi (vals[2].c_str ());
		case 2:
			month = atoi (vals[1].c_str ());
		case 1:
			year = atoi (vals[0].c_str ());
		case 0:
			printTable (year, month, day, response, response_length);
			break;
		default:
			throw rts2core::Error ("Invalid path for graph!");
	}
}

void Night::printObs (int obs_id, const char* &response_type, char* &response, int &response_length)
{
}

void Night::listObs (int year, int month, int day, std::ostringstream &_os)
{
	rts2db::ObservationSet os = rts2db::ObservationSet ();

	time_t from;
	int64_t duration;

	getNightDuration (year, month, day, from, duration);

	time_t end = from + duration;

	os.loadTime (&from, &end);

	for (rts2db::ObservationSet::iterator iter = os.begin (); iter != os.end (); iter++)
	{
		_os << "<tr><td>" << iter->getTargetName ()
			<< "</td><td>" << LibnovaDateDouble (iter->getObsStart ())
			<< "</td><td>" << LibnovaDateDouble (iter->getObsEnd ())
			<< "</td></tr>";
	}
}

void Night::printTable (int year, int month, int day, char* &response, int &response_length)
{
	bool do_list = false;
	std::ostringstream _os;

	_os << "<html><head><title>Observations";

	if (year > 0)
	{
		_os << " for " << year;
		if (month > 0)
		{
			_os << "-" << month;
			if (day > 0)
			{
				_os << "-" << day;
				do_list = true;
			}
		}
	}

	if (year == 0 || month == 0 || day == 0)
		do_list = true;

	_os << "</title></head><body><p><table>";

	if (do_list == true)
	{
		listObs (year, month, day, _os);
	}
	else
	{
		rts2db::ObservationSetDate as = rts2db::ObservationSetDate ();
		as.load (year, month, day);

		for (rts2db::ObservationSetDate::iterator iter = as.begin (); iter != as.end (); iter++)
		{
			_os << "<tr><td><a href='" << iter->first << "/'>" << iter->first << "</a></td><td>" << iter->second << "</td></tr>";
		}

	}

	_os << "</table><p></body></html>";

	response_length = _os.str ().length ();
	response = new char[response_length];
	memcpy (response, _os.str ().c_str (), response_length);
}

#endif /* HAVE_PGSQL */
