PROJ = excavator
$(PROJ).exe: $(PROJ).obj
 link /nologo /dynamicbase:no /ltcg /machine:x86 /map /merge:.rdata=.text /nxcompat /opt:icf /opt:ref /out:$(PROJ).exe /map:$(PROJ).map /release /debug /pdbaltpath:"%_PDB%" $(PROJ).obj kernel32.lib

$(PROJ).obj: $(PROJ).cpp option.hpp
 cl /nologo /c /GF /GL /GR- /GS- /Gy /MD /O1ib1 /W4 /WX /Zi /D "NDEBUG" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE" $(PROJ).cpp
