
n=64

function ret=left(m)
    ret=[m(2:end,:) ; zeros(1,size(m,2))];
    return;
endfunction

function ret=right(m)
    ret=[zeros(1,size(m,2)) ; m(1:end-1,:)];
    return;
endfunction

function ret=up(m)
    ret=[m(:,2:end)  zeros(size(m,1),1)];
    return;
endfunction

function ret=down(m)
    ret=[zeros(size(m,1),1)  m(:,1:end-1)];
    return;
endfunction

weights=ones(n,n)*4;
weights(1,:)=3;
weights(n,:)=3;
weights(:,1)=3;
weights(:,n)=3;
weights(1,1)=2;
weights(n,1)=2;
weights(1,n)=2;
weights(n,n)=2;
weights=1./weights;

privateHeat=zeros(n,n);
publicHeat=zeros(n,n);

privateHeat(1,1)=-127;
privateHeat(n,n)=127;

boundary=zeros(n,n);
boundary(1,1)=1;
boundary(n,n)=1;
boundary_lin=find(boundary);

updates=0
while 1

    privateHeatNext = left(publicHeat) + right(publicHeat) + up(publicHeat) + down(publicHeat);
    privateHeatNext = privateHeatNext .* weights;

    privateHeatNext(boundary_lin)=privateHeat(boundary_lin);
    
    privateHeat=privateHeatNext;

    update = abs(publicHeat-privateHeat) >= 0;

    printf("%f\n", sum(sum(update)))

    if updates > 1000 # && not(any(any(update)))
        break
    end

    #update = update & rand(n,n) < 0.5;
    update_lin = find(update);

    publicHeat(update_lin) = privateHeat(update_lin);

    updates=updates+1
end

imshow(publicHeat,[-127,127])