include Makefile.test
.PHONY: formant-trial
formant-trial: $(foreach source,$(COMMON_SOURCES) $(MOCK_SOURCES),$(call object_for,$(source)))
	$(CXX) $(CFLAGS) $(INCLUDES) tests/formant_trial.cpp $^ $(LDFLAGS) -o build/tests/formant-trial
