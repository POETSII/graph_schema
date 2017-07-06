pkg load geometry

## This is based on the example from
##  http://wiki.octave.org/Geometry_package

###########################################################
## Note: There may be a bug in the svg parser for octave, if you see
## a complaint about a syntax error.
##
## In:
##
##      /usr/share/octave/packages/geometry-2.1.0/io/@svg/parseSVGData.py
##
## Change this:
##
##      print 'data = struct("height",{0},"width",{1},"id","{2}");' \
##        .format(root[0].attrib['height'],root[0].attrib['width'],
##                                                          root[0].attrib['id'])
##
## to this:
## 
##   print 'data = struct("height","{0}","width","{1}","id","{2}");' \
##         .format(root[0].attrib['height'],root[0].attrib['width'],
##                                                           root[0].attrib['id'])
##


## This needs to be a really simple SVG, so only lines
## and cubic beziers, and only one blob with no holes
sourceSVG="poets.svg"

octavesvg = svg (sourceSVG).normalize();
ids       = octavesvg.pathid();
P         = octavesvg.path2polygon (ids{1}, 12)(1:end-1,:);
P         = bsxfun (@minus, P, centroid (P));


#drawPolygon (P, "-o");

## Needed?
P  = simplifypolygon(P, 'tol', 1e-3);
#drawPolygon (P, "-o");


pkg load msh

for d=[1 2 4 8]
    filename = sprintf("%s_%d", sourceSVG, d)
    meshsize = sqrt (mean (sumsq (diff (P, 1, 1), 2))) / d;
    data2geo (P, meshsize, "output", [filename ".geo"]);
    T        = msh2m_gmsh (filename);
    msh2m_gmsh_write ( sprintf("%s.msh",filename), T, {})
end

#pkg load fpl 
#graphics_toolkit ('fltk')
#pdemesh (T.p, T.e, T.t);
#view (2)
#axis tight
#axis equal

#pause
