

w=32;
h=32;

dx=1/w;
dy=1/h;

dt=0.1;

x=(1:w)*dx;
y=(1:h)*dy;

xx=repmat(x,h,1);
yy=repmat(y',1,w);

u=sin(xx)+cos(yy*2)    ;
v=zeros(w,h);

imshow(u,[0,+2]);

for i=1:1000
    L=[ u(1,:) ; u(1:end-1,:) ];
    R=[ u(2:end,:) ; u(end,:) ];
    U=[ u(:,1) , u(:,1:end-1) ];
    D=[ u(:,2:end) , u(:,end) ];
    v = v + (L+R+U+D)/4-u;
    v=v*0.99;
    u=u + v;
    
    imshow(u,[0,+2]);
    drawnow();
    
end
