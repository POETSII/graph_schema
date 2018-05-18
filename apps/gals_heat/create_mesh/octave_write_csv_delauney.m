pkg load geometry
pkg load msh

args=argv();
filename=args{1};

M = msh2m_gmsh (filename, "v", 0);

n=size(M.t,2);
c=msh2m_geometrical_properties(M, "bar");
areas=msh2m_geometrical_properties(M, "area");
neighbours=msh2m_topological_properties(M, "n");
length=msh2m_geometrical_properties(M, "slength");

t=M.t;
p=M.p;

for i=1:n
    printf("n%d   ,%g,%g,%g,%g  ",i, c(1,i),c(2,i), areas(i), sum(length(:,i)))
    for d=1:3
        printf(",%g,%g", p(1,t(d,i)),p(2,t(d,i)))
    end
    printf("   ")
    for d=1:3
        if isnan(neighbours(d,i))
            printf(",_")
        else
            printf(",n%d",neighbours(d,i))
        end
    end
    printf("\n")
end

