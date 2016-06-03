CFLAGS = -O3 -ffast-math -fno-omit-frame-pointer -Wall
LIBS =

BUILD = build

OBJFILES = \
	ggxcc.o

ggxcc.exe: $(OBJFILES:%.o=$(BUILD)/%.o)
	$(CC) -static-libgcc $(OBJFILES:%.o=$(BUILD)/%.o) $(LIBS) -o $@

default: ggxcc.exe

clean:
	rm -rf $(BUILD)/*
	rm -rf ggxcc.exe
	
$(BUILD):
	mkdir $(BUILD)
	
$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -MD -c $< -o $@
	@cp $(BUILD)/$*.d $(BUILD)/$*.P; \
		sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
		-e '/^$$/ d' -e 's/$$/ :/' < $(BUILD)/$*.d >> $(BUILD)/$*.P; \
		rm -f $(BUILD)/$*.d
	
$(BUILD)/%.o: %.cpp | $(BUILD)
	$(CXX) $(CFLAGS) -MD -c $< -o $@
	@cp $(BUILD)/$*.d $(BUILD)/$*.P; \
		sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
		-e '/^$$/ d' -e 's/$$/ :/' < $(BUILD)/$*.d >> $(BUILD)/$*.P; \
		rm -f $(BUILD)/$*.d

-include $(BUILD)/*.P