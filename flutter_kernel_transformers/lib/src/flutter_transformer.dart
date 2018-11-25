// Copyright 2018 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The kernel/src import below that requires lint `ignore_for_file`
// is a temporary state of things until kernel team builds better api that would
// replace api used below. This api was made private in an effort to discourage
// further use.
// ignore_for_file: implementation_imports
import 'package:kernel/ast.dart';
import 'package:kernel/class_hierarchy.dart';
import 'package:meta/meta.dart';
import 'package:vm/frontend_server.dart' show ProgramTransformer;

/// A base class for transforming Flutter programs.
abstract class FlutterProgramTransformer implements ProgramTransformer {

  /// The [ClassHierarchy] that should be used after applying this transformer.
  /// If any class was updated, in general we need to create a new
  /// [ClassHierarchy] instance, with new dispatch targets; or at least let
  /// the existing instance know that some of its dispatch tables are not
  /// valid anymore.
  ClassHierarchy hierarchy;

  /// The [Class] for the [Widget] type.
  Class get widgetClass => _widgetClass;
  Class _widgetClass;

  /// The [Class] for the [State] type.
  Class get stateClass => _stateClass;
  Class _stateClass;

  /// The [Class] for the [DiagnostableClass] type.
  Class get diagnostableClass => _diagnostableClass;
  Class _diagnostableClass;

  Class get locationClass => _locationClass;
  Class _locationClass;

  /// Marker interface indicating that a private _location field is
  /// available.
  Class get hasCreationLocationClass => _hasCreationLocationClass;
  Class _hasCreationLocationClass;

  /// Resolve the required Flutter classes for certain program transformations.
  @protected
  void resolveFlutterClasses(Iterable<Library> libraries) {
    // If the Widget or Debug location classes have been updated we need to get
    // the latest version
    for (Library library in libraries) {
      final Uri importUri = library.importUri;
      if (!library.isExternal &&
          importUri != null &&
          importUri.scheme == 'package') {
        if (importUri.path == 'flutter/src/widgets/framework.dart') {
          for (Class klass in library.classes) {
            if (klass.name == 'Widget') {
              _widgetClass = klass;
            } else if (klass.name == 'State') {
              _stateClass = klass;
            }
          }
        } else if (importUri.path == 'flutter/src/widgets/widget_inspector.dart') {
          for (Class class_ in library.classes) {
            if (class_.name == '_HasCreationLocation') {
              _hasCreationLocationClass = class_;
            } else if (class_.name == '_Location') {
              _locationClass = class_;
            }
          }
        } else if (importUri.path == 'flutter/src/foundation/diagnostics.dart') {
          for (Class class_ in library.classes) {
            if (class_.name == 'Diagnosticable') {
              _diagnostableClass = class_;
            }
          }
        }
      }
    }
  }

  /// Compute the [Component] for a program.
  @protected
  Component computeFullProgram(Component deltaProgram) {
    final Set<Library> libraries = new Set<Library>();
    final List<Library> workList = <Library>[];
    for (Library library in deltaProgram.libraries) {
      if (libraries.add(library)) {
        workList.add(library);
      }
    }
    while (workList.isNotEmpty) {
      final Library library = workList.removeLast();
      for (LibraryDependency dependency in library.dependencies) {
        if (libraries.add(dependency.targetLibrary)) {
          workList.add(dependency.targetLibrary);
        }
      }
    }
    return new Component()..libraries.addAll(libraries);
  }

  /// Whether `class` is a subclass of [Widget];
  @protected
  bool isSubclassOfWidget(Class clazz) {
    if (clazz == null) {
      return false;
    }
    // TODO(jacobr): use hierarchy.isSubclassOf once we are using the
    // non-deprecated ClassHierarchy constructor.
    return hierarchy.isSubclassOf(clazz, widgetClass);
  }

  /// Whether `class` is a subclass of [State];
  @protected
  bool isSubclassOfState(Class clazz) {
    if (clazz == null) {
      return false;
    }
    return hierarchy.isSubclassOf(clazz, stateClass);
  }

  /// Whether `class` is a subclass of [Diagnosticable];
  @protected
  bool isSubclassOfDiagnosticable(Class clazz) {
    if (clazz == null) {
      return false;
    }
    return hierarchy.isSubclassOf(clazz, diagnostableClass);
  }
}