/* 
 * Labels support (for targets,..).
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

#include <vector>
#include <string>

#define LABEL_PI         1
#define LABEL_PROGRAM    2

namespace rts2db
{

/**
 * Labels manipulation class.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Labels
{
	public:
		Labels () {};

		int getLabel (const char *label, int type);
		int insertLabel (const char *label, int type);
		void addLabel (int tar_id, int label_id);
		void addLabel (int tar_id, const char *label, int type, bool create);

		std::vector <std::string> getTargetLabels (int tar_id, int type);

		void deleteTargetLabels (int tar_id, int type);
};

}