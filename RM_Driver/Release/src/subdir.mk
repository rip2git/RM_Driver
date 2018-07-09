################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/COMPort.cpp \
../src/CrossSleep.cpp \
../src/Driver.cpp \
../src/IniFiles.cpp \
../src/Logger.cpp \
../src/MasterMessenger.cpp \
../src/Messenger.cpp \
../src/NamedPipe.cpp \
../src/SlaveMessenger.cpp \
../src/TON.cpp \
../src/UserInterface.cpp \
../src/main.cpp 

OBJS += \
./src/COMPort.o \
./src/CrossSleep.o \
./src/Driver.o \
./src/IniFiles.o \
./src/Logger.o \
./src/MasterMessenger.o \
./src/Messenger.o \
./src/NamedPipe.o \
./src/SlaveMessenger.o \
./src/TON.o \
./src/UserInterface.o \
./src/main.o 

CPP_DEPS += \
./src/COMPort.d \
./src/CrossSleep.d \
./src/Driver.d \
./src/IniFiles.d \
./src/Logger.d \
./src/MasterMessenger.d \
./src/Messenger.d \
./src/NamedPipe.d \
./src/SlaveMessenger.d \
./src/TON.d \
./src/UserInterface.d \
./src/main.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I../header -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


