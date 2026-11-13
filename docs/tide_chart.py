import math, re, sys
NOW = 400
PLACES = {
 "halfmoon": dict(sr="06:46", ss="17:01", ext=[(-351,-0.141),(71,1.215),(312,0.958),(670,1.69),(1130,-0.078),(1564,1.183)]),
 "sf":       dict(sr="06:47", ss="17:00", ext=[(-323,-0.134),(135,1.388),(381,1.061),(730,1.748),(1157,-0.087),(1624,1.358)]),
 "redwood":  dict(sr="06:45", ss="17:00", ext=[(-242,-0.07),(166,2.063),(463,1.146),(782,2.451),(1234,0.003),(1650,2.016)]),
}
L,R,T,B = 40,724,30,164
def svg(p):
    ext = p["ext"]; vmin = -0.5; vmax = math.ceil(max(v for _,v in ext)*2)/2 + 0.2
    X = lambda m: L + m/1440*(R-L); Y = lambda v: T + (vmax-v)/(vmax-vmin)*(B-T)
    def at(m):
        for (a,va),(b,vb) in zip(ext, ext[1:]):
            if a <= m <= b: f=(m-a)/(b-a); return va+(vb-va)*(1-math.cos(math.pi*f))/2
    def seg(m0,m1):
        ms = list(range(m0, m1, 10)) + [m1]
        return " ".join(("M" if i==0 else "L")+f"{X(m):.1f} {Y(at(m)):.1f}" for i,m in enumerate(ms))
    hm = lambda m: f"{m//60:02d}:{m%60:02d}"; mins = lambda s: int(s[:2])*60+int(s[3:])
    sr, ss = mins(p["sr"]), mins(p["ss"]); nx, ny = X(NOW), Y(at(NOW))
    o = [f'<svg width="732" height="210" viewBox="0 0 732 210" role="img" aria-label="今日潮位曲线">',
         f'<rect x="{L}" y="{T}" width="{X(sr)-L:.1f}" height="{B-T}" fill="#07111A"></rect>',
         f'<rect x="{X(ss):.1f}" y="{T}" width="{R-X(ss):.1f}" height="{B-T}" fill="#07111A"></rect>']
    for v,dash in ((0,""),(1,' stroke-dasharray="4 6"'),(2,' stroke-dasharray="4 6"')):
        o.append(f'<line x1="{L}" x2="{R}" y1="{Y(v):.1f}" y2="{Y(v):.1f}" stroke="#2A4258" stroke-width="1"{dash}></line>')
        o.append(f'<text x="34" y="{Y(v)+5:.1f}" fill="#9FB2C3" font-size="16" text-anchor="end">{v}米</text>')
    full = seg(0,1440)
    o.append(f'<path d="{full} L{R} {B} L{L} {B} Z" fill="#7FD3F2" fill-opacity="0.16"></path>')
    o.append(f'<path d="{full}" fill="none" stroke="#7FD3F2" stroke-width="3.5" stroke-linejoin="round"></path>')
    o.append(f'<text x="{X(sr)+6:.1f}" y="158" fill="#F5C451" font-size="16">日出 {p["sr"]}</text>')
    o.append(f'<text x="{X(ss)-6:.1f}" y="158" fill="#F5C451" font-size="16" text-anchor="end">日落 {p["ss"]}</text>')
    for i,(m,v) in enumerate(ext):
        if not 0 <= m <= 1440: continue
        high = v > ext[i+1][1]; cx = min(max(X(m), 44), 732-44); y = Y(v)
        y1 = y-26 if high else y-34
        col = "#7FD3F2" if high else "#F5C451"
        o.append(f'<text x="{cx:.1f}" y="{y1:.1f}" fill="{col}" font-size="18" font-weight="700" text-anchor="middle">{"满" if high else "干"} {hm(m)}</text>')
        o.append(f'<text x="{cx:.1f}" y="{y1+18:.1f}" fill="#C9D6E2" font-size="16" text-anchor="middle">{v:.2f}米</text>')
    for h,x in ((0,40),(6,211),(12,382),(18,553),(24,728)):
        o.append(f'<text x="{x}" y="204" fill="#9FB2C3" font-size="16" text-anchor="{"end" if h==24 else "middle"}">{h}点</text>')
    o.append(f'<line x1="{L}" x2="{R}" y1="{B}" y2="{B}" stroke="#3A5670" stroke-width="1.5"></line>')
    o.append('</svg>')
    return "\n          ".join(o)

blocks = []
for k,p in PLACES.items():
    blocks.append(f'<sc-if value="{{{{tide.is_{k}}}}}" hint-placeholder-val="{{{{ {"true" if k=="halfmoon" else "false"} }}}}">\n          {svg(p)}\n        </sc-if>')
new = '<div style="width: 732px; height: 210px">\n        ' + "\n        ".join(blocks) + '\n      </div>'
f = sys.argv[1]; s = open(f).read()
pat = re.compile(r'<div style="width: 732px; height: 210px">.*?</sc-if>\s*</div>', re.S)
assert len(pat.findall(s)) == 1
s = pat.sub(lambda m: new, s)
if "is_halfmoon: key" not in s: s = s.replace("labels: labels\n", "labels: labels, is_halfmoon: key === 'halfmoon', is_sf: key === 'sf', is_redwood: key === 'redwood'\n")
assert "is_halfmoon: key" in s
open(f, "w").write(s); print("ok", len(s))
