OP2_DIR = ../../../../OP2-Common/op2/c
OP2_APPS_DIR = ../../../../OP2-Common/apps/c

CPPFLAGS += -I/usr/include/mpich/
CPPFLAGS += -I$(OP2_DIR)/include
CPPFLAGS += -I/usr/include/hdf5/serial

LDFLAGS += -L$(OP2_DIR)/lib

LDLIBS += -lop2_hdf5 -lmpi -lop2_mpi

PERCENT = %


convert_mesh_to_hdf5_airfoil : CPPFLAGS += -I$(OP2_APPS_DIR)/airfoil/airfoil_hdf5/dp


meshes/airfoil_%$(PERCENT).dat : naca0012.m
	mkdir -p meshes
	octave --eval "naca0012('$@',$*/100)"

meshes/aero_%$(PERCENT).dat : naca_fem.m
	mkdir -p meshes
	octave --eval "naca_fem('$@',$*/100)"

meshes/airfoil_%.hdf5 : meshes/airfoil_%.dat convert_mesh_to_hdf5_airfoil
	LD_LIBRARY_PATH=$(OP2_DIR)/lib ./convert_mesh_to_hdf5_airfoil $< $@  | tee $*.convert.log

meshes/aero_%.hdf5 : meshes/aero_%.dat convert_mesh_to_hdf5_aero
	LD_LIBRARY_PATH=$(OP2_DIR)/lib ./convert_mesh_to_hdf5_aero $< $@  | tee $*.convert.log

%.hdf5.gz : %.hdf5
	gzip -9 $< -c > $@

%.hdf5.xz : %.hdf5
	xz -z -c $< > $@

SCALES = 1.5625% 2.2097% 3.1250% 8.8388% 6.2500% 17.6777% 12.5000%  25.0000% 35.3553% 50.0000% 70.7107% 100.0000%

all_airfoil_meshes : $(foreach s,$(SCALES),meshes/airfoil_$(s).hdf5 meshes/airfoil_$(s).hdf5.xz)

all_aero_meshes : $(foreach s,$(SCALES),meshes/aero_$(s).hdf5 meshes/aero_$(s).hdf5.xz)

all_airfoil_meshes.xz.tar : $(foreach s,$(SCALES),meshes/airfoil_$(s).hdf5.xz)
	tar -cf $@ $^

all_aero_meshes.xz.tar : $(foreach s,$(SCALES),meshes/aero_$(s).hdf5.xz)
	tar -cf $@ $^
