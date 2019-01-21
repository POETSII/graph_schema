"""
Adapted by dt10 from the following:
"""

"""
Matplotlib Animation Example

author: Jake Vanderplas
email: vanderplas@astro.washington.edu
website: http://jakevdp.github.com
license: BSD
Please feel free to use and modify this, but keep the above information. Thanks!
"""

xypos=[] # Vector of frame -> numpy particles * 2

colours={} # Map of particle num -> colour index

minx=+1000
maxx=-1000
miny=+1000
maxy=-1000
particles=set()

import numpy as np
from matplotlib import pyplot as plt
from matplotlib import animation

import sys
import csv

sourceFile='particles.csv'

if len(sys.argv)>1:
    sourceFile=sys.argv[1]

with open(sourceFile, 'r') as csvfile:
    reader = csv.reader(csvfile)
    
    xy=[] # Starts out as dict
    prevFrame=0
    
    for (frame,t,particle,colour,x,y,dx,dy) in reader:
        x=float(x)
        y=float(y)
        frame=int(frame)
        particle=int(particle)
        
        if frame!=prevFrame: # First particle in a frame
            if (frame%10)==0:
                print(" loaded frame {}".format(prevFrame))
            
            if prevFrame==0: # Just finished the first frame, now we know particle count
                xy=np.array(xy,np.single)
            xypos.append(xy)
            xy=np.empty([len(particles),2],np.single)
            prevFrame=frame 
            
        
        if frame==0:
            colour=int(colour)
            colours[particle]=colour
            particles.add(particle)
            assert particle==len(xy) # Assume contiguous
            xy.append( (x,y) )
        else:
            xy[particle,:]=(x,y)
        
        minx=min(minx,x)
        miny=min(miny,y)
        maxx=max(maxx,x)
        maxy=max(maxy,y)

print("x=({},{}), y=({},{})".format(minx,maxx,miny,maxy))



# First set up the figure, the axis, and the plot element we want to animate
fig = plt.figure()
ax = plt.axes(xlim=(minx, maxx), ylim=(miny, maxy))

palette=['r','g','b','c','y','m','y']

c=[ palette[colours[p] % len(palette)] for p in particles]
splot = ax.scatter(xypos[0][:,0],xypos[0][:,1],color=c,alpha=0.5,edgecolor='')

# initialization function: plot the background of each frame
def init():
    splot.set_offsets([])
    return splot,

# animation function.  This is called sequentially
def animate(i):
    if (i%10)==0:
        print("  Render frame {} of {}".format(i,len(xypos)))
    splot.set_offsets(xypos[i])
    return splot,

# call the animator.  blit=True means only re-draw the parts that have changed.
anim = animation.FuncAnimation(fig, animate, init_func=init,
                               frames=len(xypos), interval=20, blit=True)

# save the animation as an mp4.  This requires ffmpeg or mencoder to be
# installed.  The extra_args ensure that the x264 codec is used, so that
# the video can be embedded in html5.  You may need to adjust this for
# your system: for more information, see
# http://matplotlib.sourceforge.net/api/animation_api.html
anim.save('basic_animation.mp4', fps=25, extra_args=['-vcodec', 'libx264'])

#plt.show()
