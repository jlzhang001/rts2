/* 
 * Expanding mechanism.
 * Copyright (C) 2007-2008 Petr Kubanek <petr@kubanek.net>
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

#ifndef __RTS2_EXPANDER__
#define __RTS2_EXPANDER__

#include <string>
#include <time.h>
#include <sys/time.h>

/**
 * This class is common ancestor to expending mechanism.
 * Short one-letter variables are prefixed with %, two letters and longer
 * variables are prefixed with $. Double % or $ works as escape characters.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Rts2Expander
{
	private:
		struct tm expandDate;
		struct timeval expandTv;

		std::string getEpochString ();

		int getYear ()
		{
			return expandDate.tm_year + 1900;
		}

		int getMonth ()
		{
			return expandDate.tm_mon + 1;
		}

		int getDay ()
		{
			return expandDate.tm_mday;
		}

		int getYDay ()
		{
			return expandDate.tm_yday;
		}

		int getHour ()
		{
			return expandDate.tm_hour;
		}

		int getMin ()
		{
			return expandDate.tm_min;
		}

		int getSec ()
		{
			return expandDate.tm_sec;
		}

	protected:
		/**
		 * ID of current epoch.
		 */
		int epochId;
		virtual std::string expandVariable (char var);
		virtual std::string expandVariable (std::string expression);
	public:
		Rts2Expander ();
		Rts2Expander (const struct timeval *tv);
		Rts2Expander (Rts2Expander * in_expander);
		virtual ~ Rts2Expander (void);
		std::string expand (std::string expression);
		/**
		 * Sets expanding date to current sysdate.
		 */
		void setExpandDate ();
		
		/**
		 * Sets expanding date. This date is used in construction of
		 * the filename (for %y%d.. expansions).
		 *
		 * @param tv Timeval holding date to set.
		 */
		void setExpandDate (const struct timeval *tv);
		double getExpandDateCtime ();
		const struct timeval *getExpandDate ();

		// date related functions
		std::string getYearString ();
		std::string getMonthString ();
		std::string getDayString ();
		std::string getYDayString ();

		std::string getHourString ();
		std::string getMinString ();
		std::string getSecString ();
		std::string getMSecString ();

		long getCtimeSec ()
		{
			return expandTv.tv_sec;
		}

		long getCtimeUsec ()
		{
			return expandTv.tv_usec;
		}
};
#endif							 /* !__RTS2_EXPANDER__ */
