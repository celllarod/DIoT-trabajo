# Contiki-NG para practicas de RIS

Este repositorio es una replica del original, con las siguientes diferencias:

- [Dockerfile](../tools/docker/Dockerfile): Contiene todo lo necesario para trabajar con dongles nRF52840 (Nordic y Makerdiary)
- [Fichero CI/CD](../.gitlab-ci.yml): Encargado de montar la imagen de Docker y subirla al registro
- [Carpeta `doc`](../doc): Contenido de las prácticas
- [Fichero `.gitignore`](../.gitignore): Se permiten subir algunos tipos de ficheros antes ignorados (se dejan comentados, para referencia)

Cumple dos funciones:

- De base para la creación de la imagen Docker (se da preinstalado con la maquina virtual)
- Para hacer directamente la práctica sobre él (previo `fork` individual)

Ver [README original](README.original.md).

## Información adicional referente a la imagen de Docker

> NOTA: La información a continuación no debería necesitarse nunca, ya que se da todo preinstalado. Se deja aqui para referencia/memo.

Se ha preparado un `deploy token` para poder trabajar con la imagen de una forma ágil:

- Nombre: `vm-devel`
- Usuario: `vmu`
- Password: `dqoAp-NdCsrNYKxyAGyX`
- Expira: No
- Permisos: `read_repository, read_registry, read_package_registry`

Poniendolo todo en conjunto, para descargar la imagen usando este repositorio y credenciales propuestos:

```bash
docker login -u vmu -p dqoAp-NdCsrNYKxyAGyX registry.gitlab.com;
docker pull registry.gitlab.com/laboratorio-ris/contiki-ng:latest;
```
