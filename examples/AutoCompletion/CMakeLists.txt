add_executable(Example_AutoComplete "main.cpp")
add_qtduckdb_properties(Example_AutoComplete)
target_link_libraries(Example_AutoComplete PRIVATE Qt::Widgets)
