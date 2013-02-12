MRuby::Gem::Specification.new('mruby-tinytemplate') do |spec|
  spec.license = 'MIT'
  spec.authors = 'mattn'
  spec.linker.libraries << ['stdc++']
  spec.cxx.include_paths << ["#{build.root}/src"]
end
