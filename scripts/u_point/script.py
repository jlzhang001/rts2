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

Base class for script u_acquire.py, u_analyze.py

'''

__author__ = 'wildi.markus@bluewin.ch'

import os,time,sys
import numpy as np
from astropy import units as u
from astropy.time import Time,TimeDelta
from astropy.coordinates import SkyCoord,AltAz
import pandas as pd

from structures import NmlPosition,CatPosition,AnlPosition,cl_nms,cl_acq

class Script(object):
  def __init__(
      self,
      lg=None,
      break_after=None,
      base_path=None,
      obs=None,
      acquired_positions=None,
      analyzed_positions=None,
      acq_e_h=None,):

    self.lg=lg
    self.break_after=break_after
    self.base_path=base_path
    self.obs=obs
    self.acquired_positions=acquired_positions
    self.analyzed_positions=analyzed_positions
    self.acq_e_h=acq_e_h

  # ToDo see callback.py
  def display_fits(self,fn=None, x=None,y=None,color=None):
    ds9=ds9region.Ds9DisplayThread(debug=True,logger=self.lg)
    # Todo: ugly
    ds9.run(fn,x=x,y=y,color=color)
      
  def to_altaz(self,ic=None):
    # http://docs.astropy.org/en/stable/api/astropy.coordinates.AltAz.html
    #  Azimuth is oriented East of North (i.e., N=0, E=90 degrees)
    # RTS2 follows IAU S=0, W=90
    return ic.transform_to(AltAz(location=self.obs, pressure=0.)) # no refraction here, UTC is in cat_ic
  # ic
  def to_ic(self,aa=None):
    # strictly spoken it is not ICRS if refraction was corrected in aa
    return aa.transform_to('icrs') 

  # ToDo goes away
  def rebase_base_path(self,ptfn=None):
    if self.base_path in ptfn:
      new_ptfn=ptfn
    else:
      fn=os.path.basename(ptfn)
      new_ptfn=os.path.join(self.base_path,fn)
      self.lg.debug('rebase_base_path: file: {}'.format(new_ptfn))
    return new_ptfn
    
  def expand_base_path(self,fn=None):
    if self.base_path in fn:
      ptfn=fn
    else:
      ptfn=os.path.join(self.base_path,fn)
    return ptfn
  
  def fetch_observable_catalog(self, fn=None):
    ptfn=self.expand_base_path(fn=fn)
    self.cat=list()
    df_data = self.fetch_pandas(ptfn=ptfn,columns=['cat_no','ra','dec','mag_v',],sys_exit=True,with_nml_id=False)
    if df_data is None:
      return
    for i,rw in df_data.iterrows():
      if i > self.break_after:
        break
      
      cat_ic=SkyCoord(ra=rw['ra'],dec=rw['dec'], unit=(u.radian,u.radian), frame='icrs',obstime=self.dt_utc,location=self.obs)
      self.cat.append(CatPosition(cat_no=int(rw['cat_no']),cat_ic=cat_ic,mag_v=rw['mag_v'] ))

  def store_nominal_altaz(self,az_step=None,alt_step=None,azimuth_interval=None,altitude_interval=None,fn=None):
    # ToDo from pathlib import Path, fp=Path(ptfb),if fp.is_file())
    # format az_nml,alt_nml
    ptfn=self.expand_base_path(fn=fn)

    if os.path.isfile(ptfn):
      a=input('overwriting existing file: {} [N/y]'.format(ptfn))
      if a not in 'y':
        self.lg.info('exiting')
        sys.exit(0)

    # ToDo candidate for pandas
    with  open(ptfn, 'w') as wfl:
      self.nml=list()
      # ToDo input as int?
      az_rng=range(int(azimuth_interval[0]),int(azimuth_interval[1]),az_step) # No, exclusive + az_step
      
      alt_rng_up=range(int(altitude_interval[0]),int(altitude_interval[1]+alt_step),alt_step)
      alt_rng_down=range(int(altitude_interval[1]),int(altitude_interval[0])-alt_step,-alt_step)
      lir=len(alt_rng_up)
      up=True
      for i,az in enumerate(az_rng):
        # Epson MX-80
        if up:
          up=False
          rng=alt_rng_up
        else:
          up=True
          rng=alt_rng_down
          
        for j,alt in enumerate(rng):
          nml_id=i*lir+j
          azr=az/180.*np.pi
          altr=alt/180.*np.pi
          #          | is the id
          wfl.write('{},{},{}\n'.format(nml_id,azr,altr))
          nml_aa=SkyCoord(az=azr,alt=altr,unit=(u.radian,u.radian),frame='altaz',location=self.obs)
          self.nml.append(NmlPosition(nml_id=nml_id,nml_aa=nml_aa))

  def fetch_pandas(self, ptfn=None,columns=None,sys_exit=True,with_nml_id=True):
    # ToDo simplify that
    df_data=None
    if not os.path.isfile(ptfn):
      if sys_exit:
        self.lg.debug('fetch_pandas: {} does not exist, exiting'.format(ptfn))
        sys.exit(1)
      return None
    if os.path.getsize(ptfn)==0:
        self.lg.debug('fetch_pandas: {} file is empty, exiting'.format(ptfn))
    
    #print(columns)
    #df_data = pd.read_csv(ptfn,sep=',',names=columns,index_col='nml_id',header=None)
    if with_nml_id:
      idx='nml_id'
    else:
      idx=None
      
    try:
      df_data = pd.read_csv(ptfn,sep=',',names=columns,index_col=[idx],header=None,engine='python')
    except ValueError as e:
      self.lg.debug('fetch_pandas: {}, ValueError: {}, columnns: {}'.format(ptfn,e,columns))
      return None
    except OSError as e:
      self.lg.debug('fetch_pandas: {}, OSError: {}'.format(ptfn, e))
      return None
    except Exception as e:
      self.lg.debug('>>>>fetch_pandas: {}, Exception: {}, exiting'.format(ptfn, e))
      sys.exit(1)
        
    if with_nml_id:
      return df_data.sort_index()
    else:
      return df_data
        
  def fetch_nominal_altaz(self,fn=None):
    ptfn=self.expand_base_path(fn=fn)
    self.nml=list()
    df_data = self.fetch_pandas(ptfn=ptfn,columns=['nml_id','az','alt'],sys_exit=True)
    if df_data is None:
      return

    for i,rw in df_data.iterrows():
      nml_aa=SkyCoord(az=rw['az'],alt=rw['alt'],unit=(u.radian,u.radian),frame='altaz',location=self.obs)
      self.nml.append(NmlPosition(nml_id=i,nml_aa=nml_aa))

  def drop_nominal_altaz(self):
    obs=[int(x.nml_id)  for x in self.acq]
    observed=sorted(set(obs),reverse=True)
    for i in observed:
      del self.nml[i]
      #self.lg.debug('drop_nominal_altaz: deleted: {}'.format(i))

  def delete_one_position(self, nml_id=None,analyzed=None):
    if analyzed:
      ptfn=self.expand_base_path(fn=self.analyzed_positions)
      pos=self.anl
    else:
      ptfn=self.expand_base_path(fn=self.acquired_positions)
      pos=self.acq
      
    self.fetch_positions(analyzed=analyzed)
    for i,ps in enumerate(pos): 
      if nml_id==ps.nml_id:
        del pos[i]
        break
    else:
      self.lg.info('deleted item: item {} not found in file: {}'.format(nml_id, ptfn))
      return

    self.store_positions(analyzed=analyzed)
    self.lg.info('deleted item: {} from file: {}'.format(nml_id, ptfn))

  def store_positions(self,pos=None,analyzed=None):
    if analyzed:
      ptfn=self.expand_base_path(fn=self.analyzed_positions)
      pos=self.anl
    else:
      ptfn=self.expand_base_path(fn=self.acquired_positions)
      pos=self.acq

      if self.acq_e_h is not None:  
        while self.acq_e_h.not_writable:
          time.sleep(.1)      
    # append, one by one
    with  open(ptfn, 'w') as wfl:
      for ps in pos:
        wfl.write('{0}\n'.format(ps))
    
  def append_position(self,pos=None,analyzed=None):
    if analyzed:
      ptfn=self.expand_base_path(fn=self.analyzed_positions)
    else:
      ptfn=self.expand_base_path(fn=self.acquired_positions)
      
    # append, one by one
    with  open(ptfn, 'a') as wfl:
      wfl.write('{0}\n'.format(pos))

  def fetch_positions(self,sys_exit=None,analyzed=None):
    # ToDo!
    # dt_utc=dt_utc - TimeDelta(rw['exp']/2.,format='sec') # exp. time is small

    if analyzed:
      ptfn=self.expand_base_path(fn=self.analyzed_positions)
      pos=self.anl=list()
      cols=cl_nms
    else:
      ptfn=self.expand_base_path(fn=self.acquired_positions)
      pos=self.acq=list()    
      cols=cl_acq
    df_data = self.fetch_pandas(ptfn=ptfn,columns=cols,sys_exit=sys_exit)
    if df_data is None:
      return
    for i,rw in df_data.iterrows():
      # ToDo why not out_subfmt='fits'
      dt_begin=Time(rw['dt_begin'],format='iso', scale='utc',location=self.obs,out_subfmt='date_hms')
      dt_end=Time(rw['dt_end'],format='iso', scale='utc',location=self.obs,out_subfmt='date_hms')
      dt_end_query=Time(rw['dt_end_query'],format='iso', scale='utc',location=self.obs,out_subfmt='date_hms')

      # ToDo set best time point
      nml_aa=SkyCoord(az=rw['nml_aa_az'],alt=rw['nml_aa_alt'],unit=(u.radian,u.radian),frame='altaz',location=self.obs,obstime=dt_end)
      cat_ic=SkyCoord(ra=rw['cat_ic_ra'],dec=rw['cat_ic_dec'], unit=(u.radian,u.radian), frame='icrs',obstime=dt_end,location=self.obs)
      cat_ic_woffs=SkyCoord(ra=rw['cat_ic_woffs_ra'],dec=rw['cat_ic_woffs_dec'], unit=(u.radian,u.radian), frame='icrs',obstime=dt_end,location=self.obs)
      # replace icrs by cirs (intermediate frame, mount apparent coordinates)
      mnt_ic=SkyCoord(ra=rw['mnt_ic_ra'],dec=rw['mnt_ic_dec'], unit=(u.radian,u.radian), frame='cirs',obstime=dt_end,location=self.obs)
      mnt_aa=SkyCoord(az=rw['mnt_aa_az'],alt=rw['mnt_aa_alt'],unit=(u.radian,u.radian),frame='altaz',location=self.obs,obstime=dt_end)
      
      if 'sxtr_ra' in cols and pd.notnull(rw['sxtr_ra']) and pd.notnull(rw['sxtr_dec']):
        # ToDO icrs, cirs
        sxtr=SkyCoord(ra=rw['sxtr_ra'],dec=rw['sxtr_dec'], unit=(u.radian,u.radian), frame='icrs',obstime=dt_end,location=self.obs)
      else:
        sxtr=None
        self.lg.debug('fetch_positions: sxtr None: {}'.format(ptfn))
        
      if 'astr_ra' in cols and pd.notnull(rw['astr_ra']) and pd.notnull(rw['astr_dec']):
        # ToDO icrs, cirs
        astr=SkyCoord(ra=rw['astr_ra'],dec=rw['astr_dec'], unit=(u.radian,u.radian), frame='icrs',obstime=dt_end,location=self.obs)
        #print(cat_ic,sxtr)
        #print('DIFF c-s',(cat_ic.ra.radian-sxtr.ra.radian)*180./np.pi)
        #print('DIFF c-a',(cat_ic.ra.radian-astr.ra.radian)*180./np.pi)
        #print('DIFF s-a',(sxtr.ra.radian-astr.ra.radian)*180./np.pi)
      else:
        astr=None
        #self.lg.debug('fetch_positions: astr None,nml_d: {}, ra:{}, dec: {}'.format(i,rw['astr_ra'],rw['astr_dec']))
        # to create more or less identical plots:
        #continue
        
      image_ptfn=self.rebase_base_path(ptfn=rw['image_fn'])
      spos=AnlPosition(
          nml_id=i,
          cat_no=rw['cat_no'],
          nml_aa=nml_aa,
          cat_ic=cat_ic,
          dt_begin=dt_begin,
          dt_end=dt_end,
          dt_end_query=dt_end_query,
          JD=rw['JD'],
          cat_ic_woffs=cat_ic_woffs,
          mnt_ic=mnt_ic,
          mnt_aa=mnt_aa,
          image_fn=image_ptfn,
          exp=rw['exp'],
          pressure=rw['pressure'],
          temperature=rw['temperature'],
          humidity=rw['humidity'],
          sxtr= sxtr,
          astr= astr,
      )
      pos.append(spos)
    