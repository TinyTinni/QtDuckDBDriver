add_executable(Example_TableWidget "main.cpp")
add_qtduckdb_properties(Example_TableWidget)
target_link_libraries(Example_TableWidget PRIVATE Qt::Widgets)