def startElement( gen, ns, name, attrsb={} ):
    attrs = {}
    for i in attrsb:
        attrs[(None, i)] = attrsb[i]
    gen.startElementNS( ( ns, name ), name, attrs )
    
def endElement( gen, ns, name ):
    gen.endElementNS( ( ns, name ), name )

def writeElement( gen, ns, name, text, attrs={} ):
    startElement( gen, ns, name, attrs )
    gen.characters( text )
    endElement( gen, ns, name )
