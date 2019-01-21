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

xpos={} # Map of (particle,frame) -> x
ypos={} # Map of (particle,frame) -> y
xvel={} # Map of (particle,frame) -> dx
yvel={} # Map of (particle,frame) -> dy
tid={}
colours={} # Map of particle -> colour index

minx=+1000
maxx=-1000
miny=+1000
maxy=-1000
particles=set()
frames=0

import csv
import sys

sourceFile="particles.csv"
destFile="animation.mp4"

if len(sys.argv)>1:
    sourceFile=sys.argv[1]
if len(sys.argv)>2:
    destFile=sys.argv[2]

with open(sourceFile, 'r') as csvfile:
    reader = csv.reader(csvfile)
    for (frame,t,particle,colour,x,y,dx,dy,tinselId) in reader:
        x=float(x)
        y=float(y)
        particle=int(particle)
        colour=int(colour)
        frame=int(frame)
        tinselId=int(tinselId)
        xpos[(particle,frame)]=x
        ypos[(particle,frame)]=y
        xvel[(particle,frame)]=dx
        yvel[(particle,frame)]=dy
        tid[(particle,frame)]=tinselId
        colours[particle]=colour
        particles.add(particle)
        frames=max(int(frame)+1,frames)
        minx=min(minx,x)
        miny=min(miny,y)
        maxx=max(maxx,x)
        maxy=max(maxy,y)

print("x=({},{}), y=({},{})".format(minx,maxx,miny,maxy))

import numpy as np
from matplotlib import pyplot as plt
from matplotlib import animation

# First set up the figure, the axis, and the plot element we want to animate
fig = plt.figure()
ax = plt.axes(xlim=(minx, maxx), ylim=(miny, maxy))

palette=['r','g','b','c','y','m','y']

x=[ xpos[(p,0)] for p in particles ]
y=[ ypos[(p,0)] for p in particles ]    
c=[ palette[colours[p] % len(palette)] for p in particles]

splot = ax.scatter(x,y,c=c)

xy=[ [xpos[(i,0)],ypos[(i,0)]] for i in range(len(particles))]
ttid= [ palette[tid[(i,0)] % len(palette)] for i in range(len(particles))]

# initialization function: plot the background of each frame
def init():
    splot.set_offsets([])
    return splot,

# animation function.  This is called sequentially
def animate(i):
    # Deal with missing particles...
    for p in particles:
        if (p,i) in xpos:
            xy[p]=[ xpos[(p,i)] , ypos[(p,i)] ]
            #ttid[p]=palette[tid[(p,i)] % len(palette)]
        
    splot.set_offsets(xy)
    #splot.set_facecolors(ttid)
    return splot,

# call the animator.  blit=True means only re-draw the parts that have changed.
anim = animation.FuncAnimation(fig, animate, init_func=init,
                               frames=frames, interval=20, blit=True)

# save the animation as an mp4.  This requires ffmpeg or mencoder to be
# installed.  The extra_args ensure that the x264 codec is used, so that
# the video can be embedded in html5.  You may need to adjust this for
# your system: for more information, see
# http://matplotlib.sourceforge.net/api/animation_api.html
anim.save(destFile, fps=25, extra_args=['-vcodec', 'libx264'])

#plt.show()
