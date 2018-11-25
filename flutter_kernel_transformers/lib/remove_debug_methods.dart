// Copyright 2018 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:kernel/ast.dart';
import 'package:kernel/class_hierarchy.dart';

import 'src/flutter_transformer.dart';

/// Removes the following debug only methods:
///
///   * [State.reassemble], the hot reload hook for reinitiization.
///   * [Diagnosticable.toDiagnosticsNode], the debug formatting/printing method.
class WidgetDebugMethodRemover extends FlutterProgramTransformer {
  @override
  void transform(Component program) {
    final List<Library> libraries = program.libraries;
    if (libraries.isEmpty) {
      return;
    }
    resolveFlutterClasses(libraries);
    if (widgetClass == null) {
      return;
    }
    hierarchy = new ClassHierarchy(
      computeFullProgram(program),
      onAmbiguousSupertypes: (Class cls, Supertype a, Supertype b) { },
    );

    final Set<Class> transformedClasses = new Set<Class>.identity();
    final Set<Library> librariesToTransform = new Set<Library>.identity()
      ..addAll(libraries);

    for (Library library in libraries) {
      if (library.isExternal) {
        continue;
      }
      for (Class clazz in library.classes) {
        if (isSubclassOfState(clazz)) {
          _maybeRemoveReassemble(clazz);
        }
        if (isSubclassOfDiagnosticable(clazz)) {
          _maybeRemoveDiagnostics(clazz);
        }
      }
    }
  }

  void _maybeRemoveReassemble(Class stateClass) {
    for (Procedure procedure in stateClass.procedures) {
      if (procedure.name.name == 'reassemble') {
        procedure.remove();
      }
    }
  }

  void _maybeRemoveDiagnostics(Class widgetClass) {
    for (Procedure procedure in stateClass.procedures) {
      if (procedure.name.name == 'debugFillProperties') {
        procedure.remove();
      }
    }
  }
}
