#!/usr/bin/python3

import sys
import re
import html

keywords=set(["element","attribute","start"])
elements={}
attributes={}
groupUse={}
links={}

operators={
    "," : "Sequencing",
    "|" : "Alternation",
    "*" : "Zero or more",
    "+" : "One or more",
    "?" : "Optional",
}

comment=re.compile("^\s*#.*$")
definition=re.compile("^(\s*)([a-zA-Z]+)(\s*=.*)$")
beginElement=re.compile("^(\s*element\s+)([a-zA-Z]+)(\s+{\s*)$")
attribute=re.compile("^(\s*attribute\s+)([a-zA-Z]+)(\s+{\s*)$")
endElement=re.compile("^\s*\}.*$")
use=re.compile("^\s*([a-zA-Z]+)\s*$")

print("<html>")
print("<head>")
print("<style>")
print("  .comment { font-family:monospace; font-style: italic; color: darkgreen; white-space:pre; }")
print("  .schema { font-family: monospace; white-space:pre;  }")
#print("  .schema { background: #E0E0E0;  }")
print("  .keyword { color: darkmagenta }")
print("  .link_defn { color: blue; text-decoration:underline; text-decoration-style:dotted  }")
print("  .link_decl { color: blue; text-decoration:underline; text-decoration-style:dashed }")
print("""
// http://www.w3schools.com/css/css_tooltip.asp

/* Tooltip container */
.tooltip {
    position: relative;
    display: inline-block;
    border-bottom: 1px dotted black; /* If you want dots under the hoverable text */
}

/* Tooltip text */
.tooltip .tooltiptext {
    visibility: hidden;
    width: 120px;
    background-color: black;
    color: #fff;
    text-align: center;
    padding: 5px 0;
    border-radius: 6px;
 
    /* Position the tooltip text - see examples below! */
    position: absolute;
    z-index: 1;
}

/* Show the tooltip text when you mouse over the tooltip container */
.tooltip:hover .tooltiptext {
    visibility: visible;
}
""")
print("</style>")

lines=[]
stack=[]
for l in sys.stdin.readlines():
    l=l.rstrip("\n");
    l=html.escape(l)
    if comment.match(l):
        lines.append( (None, l) )
    else:
        defn=None
        d=definition.match(l)
        if d:
            defn=[d.group(2)]
        
        u=use.match(l)
        if u:
            tmp=list(stack+[u.group(1)])
            key="__".join(tmp)
            groupUse[key]=tmp
            sys.stderr.write("use : {} , {}\n".format(key, tmp))
            
        a=attribute.match(l)
        if a:
            defn=list(stack+["@"+a.group(2)])
            attributes["__".join(defn)]=defn
            
        e=beginElement.match(l)
        if e:
            stack.append(e.group(2))
            defn=list(stack)
            elements["__".join(list(stack))]=defn
            sys.stderr.write("Enter : {}, s={}\n".format(l,stack))

        e=endElement.match(l)
        if e:
            if len(stack)>0:
                stack.pop()
                sys.stderr.write("Leave : {}, s={}\n".format(l,stack))
            
        
        if defn:
            defn="__".join(defn)
            links[defn]=len(lines)
        lines.append( (defn,l) )
        
#print("<style>")
#for l in links:
#    print(".link_decl_{0}:hover .link_defn_{0}{{ background-color:red }}".format(l))
#    print(".link_decl_{0}:hover .link_defn_{0}{{ background-color:red }}".format(l))
#print("</style>")

print("</head>")
print("<body>")

def formatComment(defn,line):
    for k in links:
        line=line.replace(" "+k+" "," <a href='#{0}' class='link_decl link_decl_{0}'>{0}</a> ".format(k))
            
    return line

def formatCode(defn,line):
    for (o,v) in operators.items():
        line=line.replace(" "+o+" ", "<span class='tooltip'> {} <span class='tooltiptext'>{}</span></span>".format(o,v))
        
    for k in keywords:
        line=line.replace(" "+k+" "," <span class='keyword'>{}</span> ".format(k))
    for k in links:
        if k!=defn:
            line=line.replace(" "+k+" "," <a href='#{0}' class='link_decl link_decl_{0}'>{0}</a> ".format(k))
        else:
            line=line.replace(" "+k+" ", " <a id='{0}' class='link_defn'>{0}</a> ".format(k))
            
    return line

def enumChildAttributes(node):
    for e in attributes.values():
        if len(e)!=len(node)+1:
            continue
        if  e[:-1]!=node:
            continue
        yield e
        
def enumChildElements(node):
    for e in sorted(elements.values()):
        if len(e)!=len(node)+1:
            continue
        if  e[:-1]!=node:
            continue
        yield e
        
def enumChildGroupUses(node):
    for e in sorted(groupUse.values()):
        if len(e)!=len(node)+1:
            continue
        if  e[:-1]!=node:
            continue
        yield e
        
def printTree(parent, prefix):
    print("<ul>")
    for e in enumChildAttributes(parent):
        print("<li>"+prefix+e[-1]+" : attribute</li>")
    for e in enumChildGroupUses(parent):
        print("<li>"+prefix+e[-1]+" : group</li>")
    for e in enumChildElements(parent):
        print("<li>"+prefix+e[-1]+" : element</li>")
        printTree(e,prefix+"  ")
    print("</ul>")
        
        
       
        
printTree([],"")

print("<table>")
eOrder=[ (e.replace("+","/"),e) for e in links ]
eOrder.sort()
for (e1,e2) in eOrder:
    type="element"
    if e2 in attributes:
        type="attribute"
    if e2 in groupUse:
        type="group"
    print("<tr><td>{2}</td><td><a href='#{0}'>{1}</a></td></tr>".format(e2,e1,type))
print("</table>")
    

print("<div>")
for (defn,l) in lines:
    if comment.match(l):
        #g=re.match("(\s*)#[#]?(.*)", l)
        #l=g.expand(r"\1\2")
        l=formatComment(None,l)
        print('<div class="comment">{}</div>'.format(l))
    else:
        l=formatCode(defn,l)
        if defn:
            l='<div id="define_{}" class="schema link_defn_{}">{}</div>'.format(defn,defn,l)
        else:
            l='<div class="schema">{}</div>'.format(l)
        print(l)
print("</div>")
print("</body")
print("<html>")
