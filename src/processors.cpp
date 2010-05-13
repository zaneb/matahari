/* processor.cpp - Copyright (C) 2010 Red Hat, Inc.
 * Written by Darryl L. Pierce <dpierce@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.  A copy of the GNU General Public License is
 * also available at http://www.gnu.org/copyleft/gpl.html.
 */

#include "processors.h"
#include "platform.h"

void
Processors::addProcessorsListener(ProcessorsListener* listener)
{
  _listeners.insert(listener);
}

void
Processors::removeProcessorsListener(ProcessorsListener* listener)
{
  _listeners.erase(listener);
}

void
Processors::update()
{
  for(set<ProcessorsListener*>::iterator iter = _listeners.begin();
      iter != _listeners.end();
      iter++)
    {
      (*iter)->updated();
    }
}

string
Processors::getModel() const
{
  return Platform::instance()->get_processor_model();
}


unsigned int
Processors::getNumberOfCores() const
{
  return Platform::instance()->get_number_of_cores();
}

float
Processors::getLoadAverage() const
{
  return Platform::instance()->get_load_average();
}