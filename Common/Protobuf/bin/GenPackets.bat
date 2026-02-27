pushd %~dp0
protoc.exe -I=./ --cpp_out=./ ./Enum.proto
protoc.exe -I=./ --cpp_out=./ ./Struct.proto
protoc.exe -I=./ --cpp_out=./ ./InitStruct.proto
protoc.exe -I=./ --cpp_out=./ ./Protocol.proto

GenPackets.exe --path=./Protocol.proto --output=ClientPacketHandler --recv=C_ --send=S_
GenPackets.exe --path=./Protocol.proto --output=ServerPacketHandler --recv=S_ --send=C_

IF ERRORLEVEL 1 PAUSE

XCOPY /Y Enum.pb.h "../../../GameServer/GameServer"
XCOPY /Y Enum.pb.cc "../../../GameServer/GameServer"
XCOPY /Y Struct.pb.h "../../../GameServer/GameServer"
XCOPY /Y Struct.pb.cc "../../../GameServer/GameServer"
XCOPY /Y InitStruct.pb.h "../../../GameServer/GameServer"
XCOPY /Y InitStruct.pb.cc "../../../GameServer/GameServer"
XCOPY /Y Protocol.pb.h "../../../GameServer/GameServer"
XCOPY /Y Protocol.pb.cc "../../../GameServer/GameServer"
XCOPY /Y ClientPacketHandler.h "../../../GameServer/GameServer"


XCOPY /Y Enum.pb.h "../../../GameServer/DummyClient"
XCOPY /Y Enum.pb.cc "../../../GameServer/DummyClient"
XCOPY /Y Struct.pb.h "../../../GameServer/DummyClient"
XCOPY /Y Struct.pb.cc "../../../GameServer/DummyClient"
XCOPY /Y InitStruct.pb.h "../../../GameServer/DummyClient"
XCOPY /Y InitStruct.pb.cc "../../../GameServer/DummyClient"
XCOPY /Y Protocol.pb.h "../../../GameServer/DummyClient"
XCOPY /Y Protocol.pb.cc "../../../GameServer/DummyClient"
XCOPY /Y ServerPacketHandler.h "../../../GameServer/DummyClient"

XCOPY /Y Enum.pb.h "../../../PracticeProject08-9-1"
XCOPY /Y Enum.pb.cc "../../../PracticeProject08-9-1"
XCOPY /Y Struct.pb.h "../../../PracticeProject08-9-1"
XCOPY /Y Struct.pb.cc "../../../PracticeProject08-9-1"
XCOPY /Y InitStruct.pb.h "../../../PracticeProject08-9-1"
XCOPY /Y InitStruct.pb.cc "../../../PracticeProject08-9-1"
XCOPY /Y Protocol.pb.h "../../../PracticeProject08-9-1"
XCOPY /Y Protocol.pb.cc "../../../PracticeProject08-9-1"
XCOPY /Y ServerPacketHandler.h "../../../PracticeProject08-9-1"

DEL /Q /F *.pb.h
DEL /Q /F *.pb.cc
DEL /Q /F *.h
PAUSE