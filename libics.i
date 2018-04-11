/* libics.i - interface file */
%module libics
%{
#include "libics.h"
#include "libics_intern.h"
#include "libics_sensor.h"
#include "libics_ll.h"
#include "libics_test.h"
#include "config.h"
#include "libics_conf.h"
%}

%include "libics.h"
%include "libics_intern.h"
%include "libics_sensor.h"
%include "libics_ll.h"
%include "libics_test.h"
%include "config.h"
%include "libics_conf.h"
