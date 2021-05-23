function [p,q,absErr,relErr,acc] = make_rat_approx(f,range,deg_n,deg_d)

    a=chebfun(f,range)
    
    [p,q]=minimax(a,deg_n,deg_d);
    
    ae=a-p/q;
    re=ae/a;
    
    absErr=max(abs(min(ae)),abs(max(ae)));
    relErr=max(abs(min(re)),abs(max(re)));
    
    p=poly(p);
    q=poly(q);
    
    acc_p=sprintf("%.16g", p(1));
    for i = 2:length(p)
        acc_p=sprintf("(%s * x + %.16g)", acc_p, p(i));
    end
    
    acc_q=sprintf("%.16g", q(1));
    for i = 2:length(q)
        acc_q=sprintf("(%s * x + %.16g)", acc_q, q(i));
    end
    
    acc=sprintf("%s / %s", acc_p, acc_q);
end

