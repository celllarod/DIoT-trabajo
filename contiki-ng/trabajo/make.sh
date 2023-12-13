#!/bin/bash
cd servidor/ &&
make TARGET=sky servidor &&
cd ../cliente-paciente/ &&
make TARGET=z1 cliente-paciente &&
cd ../
