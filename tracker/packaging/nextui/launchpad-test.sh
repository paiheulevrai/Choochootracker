#!/bin/sh

PAK_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
LOG_DIR=${LOGS_PATH:-$PAK_DIR}

mkdir -p "$LOG_DIR"
cd "$PAK_DIR" || exit 1

exec >>"$LOG_DIR/ChooChooTracker-launchpad-test.txt" 2>&1
export CCT_NEXTUI=1
export CCT_LAUNCHPAD_TEST=1
export SDL_GAMECONTROLLERCONFIG="${SDL_GAMECONTROLLERCONFIG:+$SDL_GAMECONTROLLERCONFIG
}030003f05e0400008e02000014010000,TRIMUI Player1,a:b1,b:b0,x:b3,y:b2,back:b6,start:b7,guide:b8,leftshoulder:b4,rightshoulder:b5,leftstick:b9,rightstick:b10,lefttrigger:a2,righttrigger:a5,leftx:a0,lefty:a1,rightx:a3,righty:a4,dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,platform:Linux,"
exec ./choochootracker-launchpad-test.aarch64
