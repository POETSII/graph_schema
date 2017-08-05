AIRFOIL_WGET = wget --no-check-certificate
AIRFOIL_BASEURL = https://spo.doc.ic.ac.uk/meshes/
AIRFOIL_FULLURL= $(AIRFOIL_BASEURL)/new_grid.h5

new_grid.h5:
	$(AIRFOIL_WGET) $(AIRFOIL_FULLURL)

$(eval $(call provider_rules_template,airfoil,apps/nursery/airfoil/airfoil_graph_type.xml,no-default))
