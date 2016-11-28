# (C) 2016, Markus Wildi, wildi.markus@bluewin.ch
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2, or (at your option)
#   any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software Foundation,
#   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
#   Or visit http://www.gnu.org/licenses/gpl.html.
#

'''

positions classes for u_point

'''

__author__ = 'wildi.markus@bluewin.ch'

import numpy as np
from astropy.coordinates.representation import SphericalRepresentation

# http://scipy-cookbook.readthedocs.io/items/FittingData.html#simplifying-the-syntax
# This is the real big relief
class Parameter:
  def __init__(self, value):
    self.value = value
  def set(self, value):
    self.value = value
  def __call__(self):
    return self.value

# data structure
# u_point.py
class Point(object):
  def __init__(self,cat_lon=None,cat_lat=None,mnt_lon=None,mnt_lat=None,df_lat=None,df_lon=None,res_lat=None,res_lon=None,image_fn=None,nml_id=None):
    self.cat_lon=cat_lon
    self.cat_lat=cat_lat
    self.mnt_lon=mnt_lon
    self.mnt_lat=mnt_lat
    self.df_lat=df_lat
    self.df_lon=df_lon
    self.res_lat=res_lat
    self.res_lon=res_lon
    self.image_fn=image_fn
    self.nml_id=nml_id

class CatPosition(object):
  def __init__(self, cat_no=None,cat_ic=None,mag_v=None):
    self.cat_no=cat_no
    self.cat_ic=cat_ic
    self.mag_v=mag_v
    
# ToDo may be only a helper
class NmlPosition(object):
  def __init__(self, nml_id=None,nml_aa=None,count=1):
    self.nml_id=nml_id
    self.nml_aa=nml_aa # nominal position (grid created with store_nominal_altaz())
    self.count=count


    # ToDo a bit ugly, think about that
      
    anl_str='{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14},{15},{16},{17},{18},{19},{20},{21},{22},{23},{24}'.format(
      self.nml_id,#0
      self.cat_no,#1
      self.nml_aa.az.radian,#2
      self.nml_aa.alt.radian,#3
      self.cat_ic.ra.radian,#4
      self.cat_ic.dec.radian,#5
      self.dt_begin,#6
      self.dt_end,#7
      self.dt_end_query,#8
      self.JD,#9
      self.cat_ic_woffs.ra.radian,#10
      self.cat_ic_woffs.dec.radian,#11
      self.mnt_ic.ra.radian,#12
      self.mnt_ic.dec.radian,#13
      self.mnt_aa.az.radian,#14
      self.mnt_aa.alt.radian,#15
      self.image_fn,#16
      self.exp,#17
      self.pressure,
      self.temperature,
      self.humidity,#20
      sxtr_lon_radian,
      sxtr_lat_radian,
      astr_lon_radian,
      astr_lat_radian,#24
    )
    return anl_str

class AnlPosition(object):
  def __init__(
      self,
      nml_id=None,
      cat_no=None,
      nml_aa=None,
      cat_ic=None,
      dt_begin=None,
      dt_end=None,
      dt_end_query=None,
      JD=None,
      cat_ic_woffs=None,
      mnt_ic=None,
      mnt_aa=None,
      image_fn=None,
      exp=None,
      pressure=None,
      temperature=None,
      humidity=None,
      sxtr=None,
      astr=None):
    
    self.nml_id=nml_id
    self.cat_no=cat_no
    self.nml_aa=nml_aa # nominal position (grid created with store_nominal_altaz())
    self.cat_ic=cat_ic         # set catalog position, read back from variable ORI (rts2-mon) 
    self.dt_begin=dt_begin 
    self.dt_end=dt_end
    self.dt_end_query=dt_end_query
    self.JD=JD
    self.cat_ic_woffs=cat_ic_woffs # OFFS, offsets set manually
    # ToDo may wrong name
    self.mnt_ic=mnt_ic # TEL, read back from encodes
    self.mnt_aa=mnt_aa # TEL_ altaz 
    self.image_fn=image_fn
    self.exp=exp
    self.pressure=pressure
    self.temperature=temperature
    self.humidity=humidity
    self.sxtr=sxtr
    self.astr=astr
    
  # ToDo still ugly
  def __str__(self):
    # ToDo a bit ugly, think about that
    if self.sxtr is None:
      sxtr_lat_radian=np.nan
      sxtr_lon_radian=np.nan
    else:
      ssxtr=self.sxtr.represent_as(SphericalRepresentation)
      sxtr_lon_radian=ssxtr.lon.radian
      sxtr_lat_radian=ssxtr.lat.radian
      
    if self.astr is None:
      astr_lon_radian=np.nan
      astr_lat_radian=np.nan
    else:
      sastr=self.astr.represent_as(SphericalRepresentation)
      astr_lon_radian=sastr.lon.radian
      astr_lat_radian=sastr.lat.radian
      
    anl_str='{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14},{15},{16},{17},{18},{19},{20},{21},{22},{23},{24}'.format(
      self.nml_id,#0
      self.cat_no,#1
      self.nml_aa.az.radian,#2
      self.nml_aa.alt.radian,#3
      self.cat_ic.ra.radian,#4
      self.cat_ic.dec.radian,#5
      self.dt_begin,#6
      self.dt_end,#7
      self.dt_end_query,#8
      self.JD,#9
      self.cat_ic_woffs.ra.radian,#10
      self.cat_ic_woffs.dec.radian,#11
      self.mnt_ic.ra.radian,#12
      self.mnt_ic.dec.radian,#13
      self.mnt_aa.az.radian,#14
      self.mnt_aa.alt.radian,#15
      self.image_fn,#16
      self.exp,#17
      self.pressure,
      self.temperature,
      self.humidity,#20
      sxtr_lon_radian,
      sxtr_lat_radian,
      astr_lon_radian,
      astr_lat_radian,#24
    )
    return anl_str

# used for pandas
cl_nms= [
  'nml_id',#0
  'cat_no',#1
  'nml_aa_az',#2
  'nml_aa_alt',#3
  'cat_ic_ra',#4
  'cat_ic_dec',#5
  'dt_begin',#6
  'dt_end',#7
  'dt_end_query',#8
  'JD',#9
  'cat_ic_woffs_ra',#10
  'cat_ic_woffs_dec',#11
  'mnt_ic_ra',#12
  'mnt_ic_dec',#13
  'mnt_aa_az',#14
  'mnt_aa_alt',#15
  'image_fn',#16
  'exp',#17
  'pressure',#18
  'temperature',#19
  'humidity',#20
  'sxtr_ra',#21
  'sxtr_dec',#22
  'astr_ra',#23
  'astr_dec',#24
]

cl_acq= [
  'nml_id',#0
  'cat_no',#1
  'nml_aa_az',#2
  'nml_aa_alt',#3
  'cat_ic_ra',#4
  'cat_ic_dec',#5
  'dt_begin',#6
  'dt_end',#7
  'dt_end_query',#8
  'JD',#9
  'cat_ic_woffs_ra',#10
  'cat_ic_woffs_dec',#11
  'mnt_ic_ra',#12
  'mnt_ic_dec',#13
  'mnt_aa_az',#14
  'mnt_aa_alt',#15
  'image_fn',#16
  'exp',#17
  'pressure',#18
  'temperature',#19
  'humidity',#20
]
