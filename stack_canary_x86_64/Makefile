
# gcc is checked out to ~/gcc
# the build dir is gcc/build

CC_ARM	 = arm-linux-gnueabihf-g++
CC 		 = g++ 
INCLUDE  = -I `$(CC) -print-file-name=plugin`/include


PLUGIN_NAME	= canary
TEST_NAME	= test

$(PLUGIN_NAME).so: $(PLUGIN_NAME).cc
	$(CC) $(INCLUDE) -shared -fPIC -o $(PLUGIN_NAME).so $(PLUGIN_NAME).cc -fpermissive -fno-rtti -fPIC


$(TEST_NAME): $(PLUGIN) $(TEST_NAME).c
	$(CC) $(TEST_NAME).c -o $@ -fplugin=./$(PLUGIN_NAME).so -g3 -O0 -fno-stack-protector 

clean:
	rm $(PLUGIN_NAME).so
	rm $(TEST_NAME)
