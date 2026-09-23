#pragma once
// Historical placeholder before S3a measurement was: constexpr bool kImuAxesMeasured = false;
// Generated from the S3a six-pose drill, MEASURED on hardware 2026-09-07.
// Device: M5StickS3, board=26, firmware sha=8e57044. Drill VERDICT: PASS.
//
// Pose means (accelerometer reads +1g on whichever axis points UP):
//   1 screen facing ceiling            ( 0.004,  0.010,  1.005)  dom=z sign=+
//   2 screen facing floor              (-0.047,  0.035, -0.989)  dom=z sign=-
//   3 USB-C port to floor  (play grip) (-1.001, -0.087,  0.094)  dom=x sign=-
//   4 USB-C port to ceiling            ( 0.983, -0.151,  0.004)  dom=x sign=+
//   5 reset-button side to floor       ( 0.097,  0.980, -0.033)  dom=y sign=+
//   6 opposite side to floor           ( 0.153, -1.001, -0.044)  dom=y sign=-
//
// Therefore: X is the LONG axis, +X toward the USB-C end. Z is the FACE NORMAL,
// +Z out of the screen. Y is the cross axis. This settles the datasheet p.7
// diagram in favour of "X along the long side"; the opposing reading was wrong.
//
// LEFT/RIGHT SIGN, derived and then confirmed by measurement:
//   Owner reports the small Reset/Power button is on the LEFT long side when the
//   device is held in playing grip (screen toward player, top edge up), with the
//   USB-C end DOWN. In that grip +X points down and +Z points at the player, so by
//   the right-hand rule X x Y = Z, +Y points to the player's RIGHT.
//   Tilting the RIGHT edge toward the floor therefore drives Y NEGATIVE (pose 6,
//   ay = -1.001), and the control layer requires tilt-right to be POSITIVE.
//   Hence kTiltSignLeftRight = -1.
//
// SOFT-DROP SIGN: play grip reads ax = -1.001 (pose 3); tipping the top AWAY from
// the player rotates toward pose 1 (ax = +0.004), so ax INCREASES. Against the
// neutral captured on entering play, tipping away is a POSITIVE delta.
//   Hence kTiltSignSoftDrop = +1.

constexpr bool kImuAxesMeasured = true;
constexpr int kTiltAxisLeftRight = 1;
constexpr int8_t kTiltSignLeftRight = -1;
constexpr int kTiltAxisSoftDrop = 0;
constexpr int8_t kTiltSignSoftDrop = 1;
