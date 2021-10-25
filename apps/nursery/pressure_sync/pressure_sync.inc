

$(eval $(call provider_rules_template,pressure_sync,apps/nursery/pressure_sync/pressure_sync_graph_type.xml,no-default))

providers/pressure_sync.graph.hpp : pressure_sync_provider

apps/nursery/pressure_sync/create_pressure_sync_instance : providers/pressure_sync.graph.hpp