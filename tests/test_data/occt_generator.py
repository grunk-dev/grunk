from grunk.codegen import CodeGenerator


class MyCodeGen(CodeGenerator):

    def cpp_type_register_type(self, decl):
        if 'Standard_Transient' in decl.bases:
            return (
                "register_type<"
                + decl.node.type.spelling
                + ', opencascade_handle'
                + '>("'
                + decl.registered_name(
                    self.prefix, self.fully_qualified_names
                )
                + '")'
            )
        else:
            return super().cpp_type_register_type(decl)
            